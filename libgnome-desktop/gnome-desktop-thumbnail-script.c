/*
 * Copyright (C) 2002, 2017 Red Hat, Inc.
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Carlos Garcia Campos <carlosgc@gnome.org>
 *          Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/personality.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef ENABLE_SECCOMP
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <seccomp.h>
#endif

#include "gnome-desktop-thumbnail-script.h"

typedef enum {
  SANDBOX_TYPE_NONE,
  SANDBOX_TYPE_BWRAP,
  SANDBOX_TYPE_FLATPAK
} SandboxType;

#define GST_REGISTRY_FILENAME "gstreamer-1.0.registry"

typedef struct {
  SandboxType sandbox;
  char *thumbnailer_name;
  GArray *fd_array;
  /* Input/output file paths outside the sandbox */
  char *infile;
  char *infile_tmp; /* the host version of /tmp/gnome-desktop-file-to-thumbnail.* */
  char *outfile;
  char *outdir; /* outdir is outfile's parent dir, if it needs to be deleted */
  /* I/O file paths inside the sandbox */
  char *s_infile;
  char *s_outfile;
  /* Whether a GStreamer cache dir was setup */
  gboolean has_gst_registry;
} ScriptExec;

static char *
expand_thumbnailing_elem (const char *elem,
			  const int   size,
			  const char *infile,
			  const char *outfile,
			  gboolean   *got_input,
			  gboolean   *got_output)
{
  GString *str;
  const char *p, *last;
  char *inuri;

  str = g_string_new (NULL);

  last = elem;
  while ((p = strchr (last, '%')) != NULL)
    {
      g_string_append_len (str, last, p - last);
      p++;

      switch (*p) {
      case 'u':
        inuri = g_filename_to_uri (infile, NULL, NULL);
        if (inuri)
	  {
	    g_string_append (str, inuri);
	    *got_input = TRUE;
	    g_free (inuri);
	  }
	p++;
	break;
      case 'i':
	g_string_append (str, infile);
	*got_input = TRUE;
	p++;
	break;
      case 'o':
	g_string_append (str, outfile);
	*got_output = TRUE;
	p++;
	break;
      case 's':
	g_string_append_printf (str, "%d", size);
	p++;
	break;
      case '%':
	g_string_append_c (str, '%');
	p++;
	break;
      case 0:
      default:
	break;
      }
      last = p;
    }
  g_string_append (str, last);

  return g_string_free (str, FALSE);
}

/* From https://github.com/flatpak/flatpak/blob/master/common/flatpak-run.c */
G_GNUC_NULL_TERMINATED
static void
add_args (GPtrArray *argv_array, ...)
{
  va_list args;
  const gchar *arg;

  va_start (args, argv_array);
  while ((arg = va_arg (args, const gchar *)))
    g_ptr_array_add (argv_array, g_strdup (arg));
  va_end (args);
}

static char *
create_gst_cache_dir (void)
{
  char *out;

  out = g_build_filename (g_get_user_cache_dir (),
                          "gnome-desktop-thumbnailer",
                          "gstreamer-1.0",
                          NULL);
  if (g_mkdir_with_parents (out, 0700) < 0)
    g_clear_pointer (&out, g_free);
  return out;
}

static char *
get_extension (const char *path)
{
  g_autofree char *basename = NULL;
  char *p;

  basename = g_path_get_basename (path);
  p = strrchr (basename, '.');
  if (g_file_test (path, G_FILE_TEST_IS_DIR) ||
      !p ||
      p == basename) /* Leading periods on the basename are ignored. */
    return NULL;
  return g_strdup (p + 1);
}

#ifdef ENABLE_SECCOMP

#define FLATPAK_ERROR_SETUP_FAILED 0 /* ignored */

static gboolean
flatpak_fail (GError     **error,
	      const char  *msg,
	      ...)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, msg);
  return FALSE;
}

static gboolean
flatpak_fail_error (GError     **error,
                    int          unused_error,
	            const char  *msg,
	            ...)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, msg);
  return FALSE;
}

/* From https://github.com/flatpak/flatpak/blob/master/common/flatpak-utils.c */
#if !defined(__i386__) && !defined(__x86_64__) && !defined(__aarch64__) && !defined(__arm__)
static const char *
flatpak_get_kernel_arch (void)
{
  static struct utsname buf;
  static char *arch = NULL;
  char *m;

  if (arch != NULL)
    return arch;

  if (uname (&buf))
    {
      arch = "unknown";
      return arch;
    }

  /* By default, just pass on machine, good enough for most arches */
  arch = buf.machine;

  /* Override for some arches */

  m = buf.machine;
  /* i?86 */
  if (strlen (m) == 4 && m[0] == 'i' && m[2] == '8'  && m[3] == '6')
    {
      arch = "i386";
    }
  else if (g_str_has_prefix (m, "arm"))
    {
      if (g_str_has_suffix (m, "b"))
        arch = "armeb";
      else
        arch = "arm";
    }
  else if (strcmp (m, "mips") == 0)
    {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      arch = "mipsel";
#endif
    }
  else if (strcmp (m, "mips64") == 0)
    {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      arch = "mips64el";
#endif
    }

  return arch;
}
#endif

/* This maps the kernel-reported uname to a single string representing
 * the cpu family, in the sense that all members of this family would
 * be able to understand and link to a binary file with such cpu
 * opcodes. That doesn't necessarily mean that all members of the
 * family can run all opcodes, for instance for modern 32bit intel we
 * report "i386", even though they support instructions that the
 * original i386 cpu cannot run. Still, such an executable would
 * at least try to execute a 386, whereas an arm binary would not.
 */
static const char *
flatpak_get_arch (void)
{
  /* Avoid using uname on multiarch machines, because uname reports the kernels
   * arch, and that may be different from userspace. If e.g. the kernel is 64bit and
   * the userspace is 32bit we want to use 32bit by default. So, we take the current build
   * arch as the default. */
#if defined(__i386__)
  return "i386";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__aarch64__)
  return "aarch64";
#elif defined(__arm__)
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  return "arm";
#else
  return "armeb";
#endif
#else
  return flatpak_get_kernel_arch ();
#endif
}

/* From https://github.com/flatpak/flatpak/blob/master/common/flatpak-run.c */
static const guint32 seccomp_x86_64_extra_arches[] = { SCMP_ARCH_X86, 0, };

#ifdef SCMP_ARCH_AARCH64
static const guint32 seccomp_aarch64_extra_arches[] = { SCMP_ARCH_ARM, 0 };
#endif

/*
 * @negative_errno: Result code as returned by libseccomp functions
 *
 * Translate a libseccomp error code into an error message. libseccomp
 * mostly returns negative `errno` values such as `-ENOMEM`, but some
 * standard `errno` values are used for non-standard purposes where their
 * `strerror()` would be misleading.
 *
 * Returns: a string version of @negative_errno if possible
 */
static const char *
flatpak_seccomp_strerror (int negative_errno)
{
  g_return_val_if_fail (negative_errno < 0, "Non-negative error value from libseccomp?");
  g_return_val_if_fail (negative_errno > INT_MIN, "Out of range error value from libseccomp?");

  switch (negative_errno)
    {
      case -EDOM:
        return "Architecture specific failure";

      case -EFAULT:
        return "Internal libseccomp failure (unknown syscall?)";

      case -ECANCELED:
        return "System failure beyond the control of libseccomp";
    }

  /* e.g. -ENOMEM: the result of strerror() is good enough */
  return g_strerror (-negative_errno);
}

static inline void
cleanup_seccomp (void *p)
{
  scmp_filter_ctx *pp = (scmp_filter_ctx *) p;

  if (*pp)
    seccomp_release (*pp);
}

static gboolean
setup_seccomp (GPtrArray  *argv_array,
               GArray     *fd_array,
               const char *arch,
               gulong      allowed_personality,
               gboolean    multiarch,
               gboolean    devel,
               GError    **error)
{
  __attribute__((cleanup (cleanup_seccomp))) scmp_filter_ctx seccomp = NULL;

  /**** BEGIN NOTE ON CODE SHARING
   *
   * This code is copied from Flatpak:
   *
   *   https://github.com/flatpak/flatpak/blob/main/common/flatpak-run.c
   *
   * It should be routinely updated to account for changes in Flatpak.
   *
   * We ought to split this code out of Flatpak into a subproject, to make
   * make code sharing easier and reduce the need for manual copy/pasting.
   *
   **** END NOTE ON CODE SHARING
   */
  struct
  {
    int                  scall;
    int                  errnum;
    struct scmp_arg_cmp *arg;
  } syscall_blocklist[] = {
    /* Block dmesg */
    {SCMP_SYS (syslog), EPERM},
    /* Useless old syscall */
    {SCMP_SYS (uselib), EPERM},
    /* Don't allow disabling accounting */
    {SCMP_SYS (acct), EPERM},
    /* Don't allow reading current quota use */
    {SCMP_SYS (quotactl), EPERM},

    /* Don't allow access to the kernel keyring */
    {SCMP_SYS (add_key), EPERM},
    {SCMP_SYS (keyctl), EPERM},
    {SCMP_SYS (request_key), EPERM},

    /* Scary VM/NUMA ops */
    {SCMP_SYS (move_pages), EPERM},
    {SCMP_SYS (mbind), EPERM},
    {SCMP_SYS (get_mempolicy), EPERM},
    {SCMP_SYS (set_mempolicy), EPERM},
    {SCMP_SYS (migrate_pages), EPERM},

    /* Don't allow subnamespace setups: */
    {SCMP_SYS (unshare), EPERM},
    {SCMP_SYS (setns), EPERM},
    {SCMP_SYS (mount), EPERM},
    {SCMP_SYS (umount), EPERM},
    {SCMP_SYS (umount2), EPERM},
    {SCMP_SYS (pivot_root), EPERM},
    {SCMP_SYS (chroot), EPERM},
#if defined(__s390__) || defined(__s390x__) || defined(__CRIS__)
    /* Architectures with CONFIG_CLONE_BACKWARDS2: the child stack
     * and flags arguments are reversed so the flags come second */
    {SCMP_SYS (clone), EPERM, &SCMP_A1 (SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER)},
#else
    /* Normally the flags come first */
    {SCMP_SYS (clone), EPERM, &SCMP_A0 (SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER)},
#endif

    /* Don't allow faking input to the controlling tty (CVE-2017-5226) */
    {SCMP_SYS (ioctl), EPERM, &SCMP_A1 (SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, (int) TIOCSTI)},
    /* In the unlikely event that the controlling tty is a Linux virtual
     * console (/dev/tty2 or similar), copy/paste operations have an effect
     * similar to TIOCSTI (CVE-2023-28100) */
    {SCMP_SYS (ioctl), EPERM, &SCMP_A1 (SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, (int) TIOCLINUX)},

    /* seccomp can't look into clone3()'s struct clone_args to check whether
     * the flags are OK, so we have no choice but to block clone3().
     * Return ENOSYS so user-space will fall back to clone().
     * (CVE-2021-41133; see also https://github.com/moby/moby/commit/9f6b562d) */
    {SCMP_SYS (clone3), ENOSYS},

    /* New mount manipulation APIs can also change our VFS. There's no
     * legitimate reason to do these in the sandbox, so block all of them
     * rather than thinking about which ones might be dangerous.
     * (CVE-2021-41133) */
    {SCMP_SYS (open_tree), ENOSYS},
    {SCMP_SYS (move_mount), ENOSYS},
    {SCMP_SYS (fsopen), ENOSYS},
    {SCMP_SYS (fsconfig), ENOSYS},
    {SCMP_SYS (fsmount), ENOSYS},
    {SCMP_SYS (fspick), ENOSYS},
    {SCMP_SYS (mount_setattr), ENOSYS},
  };

  struct
  {
    int                  scall;
    int                  errnum;
    struct scmp_arg_cmp *arg;
  } syscall_nondevel_blocklist[] = {
    /* Profiling operations; we expect these to be done by tools from outside
     * the sandbox.  In particular perf has been the source of many CVEs.
     */
    {SCMP_SYS (perf_event_open), EPERM},
    /* Don't allow you to switch to bsd emulation or whatnot */
    {SCMP_SYS (personality), EPERM, &SCMP_A0 (SCMP_CMP_NE, allowed_personality)},
    {SCMP_SYS (ptrace), EPERM}
  };
  /* Blocklist all but unix, inet, inet6 and netlink */
  struct
  {
    int             family;
    int /* FlatpakRunFlags */ unused_flags_mask;
  } socket_family_allowlist[] = {
    /* NOTE: Keep in numerical order */
    { AF_UNSPEC, 0 },
    { AF_LOCAL, 0 },
    { AF_INET, 0 },
    { AF_INET6, 0 },
    { AF_NETLINK, 0 },
    /* { AF_CAN, FLATPAK_RUN_FLAG_CANBUS }, */
    /* { AF_BLUETOOTH, FLATPAK_RUN_FLAG_BLUETOOTH }, */
  };
  int last_allowed_family;
  int i, r;
  int fd = -1;
  g_autofree char *fd_str = NULL;
  g_autofree char *path = NULL;

  seccomp = seccomp_init (SCMP_ACT_ALLOW);
  if (!seccomp)
    return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Initialize seccomp failed"));

  if (arch != NULL)
    {
      uint32_t arch_id = 0;
      const uint32_t *extra_arches = NULL;

      if (strcmp (arch, "i386") == 0)
        {
          arch_id = SCMP_ARCH_X86;
        }
      else if (strcmp (arch, "x86_64") == 0)
        {
          arch_id = SCMP_ARCH_X86_64;
          extra_arches = seccomp_x86_64_extra_arches;
        }
      else if (strcmp (arch, "arm") == 0)
        {
          arch_id = SCMP_ARCH_ARM;
        }
#ifdef SCMP_ARCH_AARCH64
      else if (strcmp (arch, "aarch64") == 0)
        {
          arch_id = SCMP_ARCH_AARCH64;
          extra_arches = seccomp_aarch64_extra_arches;
        }
#endif

      /* We only really need to handle arches on multiarch systems.
       * If only one arch is supported the default is fine */
      if (arch_id != 0)
        {
          /* This *adds* the target arch, instead of replacing the
             native one. This is not ideal, because we'd like to only
             allow the target arch, but we can't really disallow the
             native arch at this point, because then bubblewrap
             couldn't continue running. */
          r = seccomp_arch_add (seccomp, arch_id);
          if (r < 0 && r != -EEXIST)
            return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to add architecture to seccomp filter: %s"), flatpak_seccomp_strerror (r));

          if (multiarch && extra_arches != NULL)
            {
              for (i = 0; extra_arches[i] != 0; i++)
                {
                  r = seccomp_arch_add (seccomp, extra_arches[i]);
                  if (r < 0 && r != -EEXIST)
                    return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to add multiarch architecture to seccomp filter: %s"), flatpak_seccomp_strerror (r));
                }
            }
        }
    }

  /* TODO: Should we filter the kernel keyring syscalls in some way?
   * We do want them to be used by desktop apps, but they could also perhaps
   * leak system stuff or secrets from other apps.
   */

  for (i = 0; i < G_N_ELEMENTS (syscall_blocklist); i++)
    {
      int scall = syscall_blocklist[i].scall;
      int errnum = syscall_blocklist[i].errnum;

      g_return_val_if_fail (errnum == EPERM || errnum == ENOSYS, FALSE);

      if (syscall_blocklist[i].arg)
        r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 1, *syscall_blocklist[i].arg);
      else
        r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 0);

      /* EFAULT means "internal libseccomp error", but in practice we get
       * this for syscall numbers added via flatpak-syscalls-private.h
       * when trying to filter them on a non-native architecture, because
       * libseccomp cannot map the syscall number to a name and back to a
       * number for the non-native architecture. */
      if (r == -EFAULT)
        g_debug ("Unable to block syscall %d: syscall not known to libseccomp?",
                 scall);
      else if (r < 0)
        return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to block syscall %d: %s"), scall, flatpak_seccomp_strerror (r));
    }

  if (!multiarch)
    {
      /* modify_ldt is a historic source of interesting information leaks,
       * so it's disabled as a hardening measure.
       * However, it is required to run old 16-bit applications
       * as well as some Wine patches, so it's allowed in multiarch. */
      int scall = SCMP_SYS (modify_ldt);
      r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (EPERM), scall, 0);

      /* See above for the meaning of EFAULT. */
      if (r == -EFAULT)
        g_debug ("Unable to block syscall %d: syscall not known to libseccomp?",
                 scall);
      else if (r < 0)
        return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to block syscall %d: %s"), scall, flatpak_seccomp_strerror (r));
    }

  if (!devel)
    {
      for (i = 0; i < G_N_ELEMENTS (syscall_nondevel_blocklist); i++)
        {
          int scall = syscall_nondevel_blocklist[i].scall;
          int errnum = syscall_nondevel_blocklist[i].errnum;

          g_return_val_if_fail (errnum == EPERM || errnum == ENOSYS, FALSE);

          if (syscall_nondevel_blocklist[i].arg)
            r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 1, *syscall_nondevel_blocklist[i].arg);
          else
            r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 0);

          /* See above for the meaning of EFAULT. */
          if (r == -EFAULT)
            g_debug ("Unable to block syscall %d: syscall not known to libseccomp?",
                     scall);
          else if (r < 0)
            return flatpak_fail_error (error, FLATPAK_ERROR_SETUP_FAILED, _("Failed to block syscall %d: %s"), scall, flatpak_seccomp_strerror (r));
        }
    }

  /* Socket filtering doesn't work on e.g. i386, so ignore failures here
   * However, we need to user seccomp_rule_add_exact to avoid libseccomp doing
   * something else: https://github.com/seccomp/libseccomp/issues/8 */
  last_allowed_family = -1;
  for (i = 0; i < G_N_ELEMENTS (socket_family_allowlist); i++)
    {
      int family = socket_family_allowlist[i].family;
      int disallowed;

/*
      if (socket_family_allowlist[i].flags_mask != 0 &&
          (socket_family_allowlist[i].flags_mask & run_flags) != socket_family_allowlist[i].flags_mask)
        continue;
 */

      for (disallowed = last_allowed_family + 1; disallowed < family; disallowed++)
        {
          /* Blocklist the in-between valid families */
          seccomp_rule_add_exact (seccomp, SCMP_ACT_ERRNO (EAFNOSUPPORT), SCMP_SYS (socket), 1, SCMP_A0 (SCMP_CMP_EQ, disallowed));
        }
      last_allowed_family = family;
    }
  /* Blocklist the rest */
  seccomp_rule_add_exact (seccomp, SCMP_ACT_ERRNO (EAFNOSUPPORT), SCMP_SYS (socket), 1, SCMP_A0 (SCMP_CMP_GE, last_allowed_family + 1));

  /* Code below this point is based on an earlier version of flatpak-run.c. */

  fd = g_file_open_tmp ("flatpak-seccomp-XXXXXX", &path, error);
  if (fd == -1)
    return FALSE;

  unlink (path);

  if (seccomp_export_bpf (seccomp, fd) != 0)
    {
      close (fd);
      return flatpak_fail (error, "Failed to export bpf");
    }

  lseek (fd, 0, SEEK_SET);

  fd_str = g_strdup_printf ("%d", fd);
  if (fd_array)
    g_array_append_val (fd_array, fd);

  add_args (argv_array,
            "--seccomp", fd_str,
            NULL);

  fd = -1; /* Don't close on success */

  return TRUE;
}
#endif

#ifdef HAVE_BWRAP
static gboolean
path_is_usrmerged (const char *dir)
{
  /* does /dir point to /usr/dir? */
  g_autofree char *target = NULL;
  GStatBuf stat_buf_src, stat_buf_target;

  if (g_stat (dir, &stat_buf_src) < 0)
    return FALSE;

  target = g_strdup_printf ("/usr/%s", dir);

  if (g_stat (target, &stat_buf_target) < 0)
    return FALSE;

  return (stat_buf_src.st_dev == stat_buf_target.st_dev) &&
         (stat_buf_src.st_ino == stat_buf_target.st_ino);
}

static void
add_bwrap_env (GPtrArray  *array,
               const char *envvar)
{
  if (g_getenv (envvar) != NULL)
    add_args (array,
              "--setenv", envvar, g_getenv (envvar),
              NULL);
}

static gboolean
add_bwrap (GPtrArray   *array,
	   ScriptExec  *script)
{
  const char * const usrmerged_dirs[] = { "bin", "lib64", "lib", "sbin" };
  int i;
  g_autofree char *gst_cache_dir = NULL;

  g_return_val_if_fail (script->outdir != NULL, FALSE);
  g_return_val_if_fail (script->s_infile != NULL, FALSE);

  add_args (array,
	    "bwrap",
	    "--ro-bind", "/usr", "/usr",
	    "--ro-bind-try", "/etc/ld.so.cache", "/etc/ld.so.cache",
	    NULL);

  /* These directories might be symlinks into /usr/... */
  for (i = 0; i < G_N_ELEMENTS (usrmerged_dirs); i++)
    {
      g_autofree char *absolute_dir = g_strdup_printf ("/%s", usrmerged_dirs[i]);

      if (!g_file_test (absolute_dir, G_FILE_TEST_EXISTS))
        continue;

      if (path_is_usrmerged (absolute_dir))
        {
          g_autofree char *symlink_target = g_strdup_printf ("/usr/%s", absolute_dir);

          add_args (array,
                    "--symlink", symlink_target, absolute_dir,
                    NULL);
        }
      else
        {
          add_args (array,
                    "--ro-bind", absolute_dir, absolute_dir,
                    NULL);
        }
    }

  /* fontconfig cache if necessary */
  if (!g_str_has_prefix (FONTCONFIG_CACHE_PATH, "/usr/"))
    add_args (array, "--ro-bind-try", FONTCONFIG_CACHE_PATH, FONTCONFIG_CACHE_PATH, NULL);

  /* GStreamer plugin cache if possible */
  gst_cache_dir = create_gst_cache_dir ();
  if (gst_cache_dir)
    {
      g_autofree char *registry = NULL;
      script->has_gst_registry = TRUE;
      registry = g_build_filename (gst_cache_dir, GST_REGISTRY_FILENAME, NULL);
      add_args (array,
                "--setenv", "GST_REGISTRY_1_0", registry,
                "--bind", gst_cache_dir, gst_cache_dir,
                NULL);
    }

  /*
   * Used in various distributions. On those distributions, /usr is not
   * complete without it: some files in /usr might be a symbolic link
   * like /usr/bin/composite -> /etc/alternatives/composite ->
   * /usr/bin/composite-im6.q16.
   *
   * https://manpages.debian.org/stable/dpkg/update-alternatives.1.en.html
   * https://docs.fedoraproject.org/en-US/packaging-guidelines/Alternatives/
   * https://en.opensuse.org/openSUSE:Packaging_Multiple_Version_guidelines
   */
  add_args (array, "--ro-bind-try", "/etc/alternatives", "/etc/alternatives", NULL);

  add_args (array,
	    "--proc", "/proc",
	    "--dev", "/dev",
	    "--chdir", "/",
	    "--setenv", "GIO_USE_VFS", "local",
	    "--unshare-all",
	    "--die-with-parent",
	    NULL);

  add_bwrap_env (array, "G_MESSAGES_DEBUG");
  add_bwrap_env (array, "G_MESSAGES_PREFIXED");
  add_bwrap_env (array, "GST_DEBUG");

  /* Add gnome-desktop's install prefix if needed */
  if (g_strcmp0 (INSTALL_PREFIX, "") != 0 &&
      g_strcmp0 (INSTALL_PREFIX, "/usr") != 0 &&
      g_strcmp0 (INSTALL_PREFIX, "/usr/") != 0)
    {
      add_args (array,
                "--ro-bind", INSTALL_PREFIX, INSTALL_PREFIX,
                NULL);
    }

  g_ptr_array_add (array, g_strdup ("--bind"));
  g_ptr_array_add (array, g_strdup (script->outdir));
  g_ptr_array_add (array, g_strdup ("/tmp"));

  /* We make sure to also re-use the original file's original
   * extension in case it's useful for the thumbnailer to
   * identify the file type */
  g_ptr_array_add (array, g_strdup ("--ro-bind"));
  g_ptr_array_add (array, g_strdup (script->infile));
  g_ptr_array_add (array, g_strdup (script->s_infile));

  return TRUE;
}
#endif /* HAVE_BWRAP */

static void
add_flatpak_env (GPtrArray  *array,
                 const char *envvar)
{
  if (g_getenv (envvar) != NULL)
    {
      g_autofree char *option = NULL;

      option = g_strdup_printf ("--env=%s=%s",
                                envvar,
                                g_getenv (envvar));
      add_args (array, option, NULL);
    }
}

static gboolean
add_flatpak (GPtrArray   *array,
             ScriptExec  *script)
{
  g_autofree char *inpath = NULL;
  g_autofree char *outpath = NULL;
  g_autofree char *gst_cache_dir = NULL;

  g_return_val_if_fail (script->outdir != NULL, FALSE);
  g_return_val_if_fail (script->infile != NULL, FALSE);

  add_args (array,
            "flatpak-spawn",
            "--clear-env",
            "--env=GIO_USE_VFS=local",
            "--env=LC_ALL=C.UTF-8",
            NULL);

  add_flatpak_env (array, "G_MESSAGES_DEBUG");
  add_flatpak_env (array, "G_MESSAGES_PREFIXED");
  add_flatpak_env (array, "GST_DEBUG");

  /* GStreamer plugin cache if possible */
  gst_cache_dir = create_gst_cache_dir ();
  if (gst_cache_dir)
    {
      g_autofree char *registry_path = NULL;
      g_autofree char *env_option = NULL;
      g_autofree char *expose_path_option = NULL;

      script->has_gst_registry = TRUE;
      registry_path = g_build_filename (gst_cache_dir, GST_REGISTRY_FILENAME, NULL);
      expose_path_option = g_strdup_printf ("--sandbox-expose-path=%s", gst_cache_dir);
      env_option = g_strdup_printf ("--env=GST_REGISTRY_1_0=%s", registry_path);
      add_args (array, expose_path_option, env_option, NULL);
    }

  outpath = g_strdup_printf ("--sandbox-expose-path=%s", script->outdir);
  inpath = g_strdup_printf ("--sandbox-expose-path-ro=%s", script->infile);

  add_args (array,
            "--watch-bus",
            "--sandbox",
            "--no-network",
            outpath,
            inpath,
            NULL);

  return TRUE;
}

static char **
expand_thumbnailing_cmd (const char  *cmd,
			 ScriptExec  *script,
			 int          size,
			 GError     **error)
{
  GPtrArray *array;
  g_auto(GStrv) cmd_elems = NULL;
  guint i;
  gboolean got_in, got_out;

  if (!g_shell_parse_argv (cmd, NULL, &cmd_elems, error))
    return NULL;

  script->thumbnailer_name = g_strdup (cmd_elems[0]);

  array = g_ptr_array_new_with_free_func (g_free);

#ifdef HAVE_BWRAP
  if (script->sandbox == SANDBOX_TYPE_BWRAP)
    {
      if (!add_bwrap (array, script))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			       "Bubblewrap setup failed");
	  goto bail;
	}
    }
#endif

#ifdef ENABLE_SECCOMP
  if (script->sandbox == SANDBOX_TYPE_BWRAP)
    {
      const char *arch;

      arch = flatpak_get_arch ();
      g_assert (arch);
      if (!setup_seccomp (array,
                          script->fd_array,
                          arch,
                          PER_LINUX,
                          FALSE,
                          FALSE,
                          error))
        {
          goto bail;
        }
    }
#endif

  if (script->sandbox == SANDBOX_TYPE_FLATPAK)
    {
      if (!add_flatpak (array, script))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "Flatpak-spawn setup failed");
          goto bail;
        }
    }

  got_in = got_out = FALSE;
  for (i = 0; cmd_elems[i] != NULL; i++)
    {
      char *expanded;

      expanded = expand_thumbnailing_elem (cmd_elems[i],
					   size,
					   script->s_infile ? script->s_infile : script->infile,
					   script->s_outfile ? script->s_outfile : script->outfile,
					   &got_in,
					   &got_out);

      g_ptr_array_add (array, expanded);
    }

  if (!got_in)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   "Input file could not be set");
      goto bail;
    }
  else if (!got_out)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   "Output file could not be set");
      goto bail;
    }

  g_ptr_array_add (array, NULL);

  return (char **) g_ptr_array_free (array, FALSE);

bail:
  g_ptr_array_free (array, TRUE);
  return NULL;
}

static void
child_setup (gpointer user_data)
{
  GArray *fd_array = user_data;
  guint i;

  /* If no fd_array was specified, don't care. */
  if (fd_array == NULL)
    return;

  /* Otherwise, mark not - close-on-exec all the fds in the array */
  for (i = 0; i < fd_array->len; i++)
    fcntl (g_array_index (fd_array, int, i), F_SETFD, 0);
}

static void
script_exec_free (ScriptExec *exec)
{
  if (exec == NULL)
    return;

  g_free (exec->thumbnailer_name);
  g_free (exec->infile);
  if (exec->infile_tmp)
    {
      if (g_file_test (exec->infile_tmp, G_FILE_TEST_IS_DIR))
        g_rmdir (exec->infile_tmp);
      else
        g_unlink (exec->infile_tmp);
      g_free (exec->infile_tmp);
    }
  if (exec->outfile)
    {
      g_unlink (exec->outfile);
      g_free (exec->outfile);
    }
  if (exec->outdir)
    {
      if (g_rmdir (exec->outdir) < 0)
        {
          g_warning ("Could not remove %s, thumbnailer %s left files in directory",
                     exec->outdir, exec->thumbnailer_name);
        }
      g_free (exec->outdir);
    }
  g_free (exec->s_infile);
  g_free (exec->s_outfile);
  if (exec->fd_array)
    g_array_free (exec->fd_array, TRUE);
  g_free (exec);
}

static void
clear_fd (gpointer data)
{
  int *fd_p = data;
  if (fd_p != NULL && *fd_p != -1)
    close (*fd_p);
}

static guint32
get_portal_version (void)
{
  static guint32 version = G_MAXUINT32;

  if (version == G_MAXUINT32)
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GDBusConnection) session_bus =
        g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
      g_autoptr(GVariant) reply = NULL;

      if (session_bus)
        reply = g_dbus_connection_call_sync (session_bus,
                                             "org.freedesktop.portal.Flatpak",
                                             "/org/freedesktop/portal/Flatpak",
                                             "org.freedesktop.DBus.Properties",
                                             "Get",
                                             g_variant_new ("(ss)", "org.freedesktop.portal.Flatpak", "version"),
                                             G_VARIANT_TYPE ("(v)"),
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL, &error);

      if (reply == NULL)
        {
          g_debug ("Failed to get Flatpak portal version: %s", error->message);
          /* Don't try again if we failed once */
          version = 0;
        }
      else
        {
          g_autoptr(GVariant) v = g_variant_get_child_value (reply, 0);
          g_autoptr(GVariant) v2 = g_variant_get_variant (v);
          version = g_variant_get_uint32 (v2);
        }
    }

  return version;
}

static ScriptExec *
script_exec_new (const char  *uri,
		 GError     **error)
{
  ScriptExec *exec;
  g_autoptr(GFile) file = NULL;

  exec = g_new0 (ScriptExec, 1);
#ifdef HAVE_BWRAP
  /* Bubblewrap is not used if the application is already sandboxed in
   * Flatpak as all privileges to create a new namespace are dropped when
   * the initial one is created. */
  if (g_file_test ("/.flatpak-info", G_FILE_TEST_IS_REGULAR))
    {
      if (get_portal_version () >= 3)
        exec->sandbox = SANDBOX_TYPE_FLATPAK;
      else
        exec->sandbox = SANDBOX_TYPE_NONE;
    }
  /* Similarly for snaps */
  else if (g_getenv ("SNAP_NAME") != NULL)
    exec->sandbox = SANDBOX_TYPE_NONE;
  else
    exec->sandbox = SANDBOX_TYPE_BWRAP;
#endif

  file = g_file_new_for_uri (uri);

  exec->infile = g_file_get_path (file);
  if (!exec->infile)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "Could not get path for URI '%s'", uri);
      goto bail;
    }

#ifdef HAVE_BWRAP
  if (exec->sandbox == SANDBOX_TYPE_BWRAP)
    {
      char *tmpl;
      g_autofree char *ext = NULL;
      g_autofree char *infile = NULL;

      exec->fd_array = g_array_new (FALSE, TRUE, sizeof (int));
      g_array_set_clear_func (exec->fd_array, clear_fd);

      tmpl = g_strdup ("/tmp/gnome-desktop-thumbnailer-XXXXXX");
      exec->outdir = g_mkdtemp (tmpl);
      if (!exec->outdir)
        {
          int errsv = errno;

          g_clear_pointer (&tmpl, g_free);
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                       "Could not create temporary sandbox directory: %s",
                       g_strerror (errsv));
          goto bail;
        }
      exec->outfile = g_build_filename (exec->outdir, "gnome-desktop-thumbnailer.png", NULL);
      ext = get_extension (exec->infile);

      if (ext)
        infile = g_strdup_printf ("gnome-desktop-file-to-thumbnail.%s", ext);
      else
        infile = g_strdup_printf ("gnome-desktop-file-to-thumbnail");

      exec->infile_tmp = g_build_filename (exec->outdir, infile, NULL);

      exec->s_infile = g_build_filename ("/tmp/", infile, NULL);
      exec->s_outfile = g_build_filename ("/tmp/", "gnome-desktop-thumbnailer.png", NULL);
    }
  else
#endif
  if (exec->sandbox == SANDBOX_TYPE_FLATPAK)
    {
      char *tmpl;
      const char *sandbox_dir;
      g_autofree char *resolved_infile = NULL;

      /* Make sure any symlinks in the path are resolved, because the resolved
       * path is what's exposed by Flatpak into the sandbox */
      resolved_infile = realpath (exec->infile, NULL);
      if (!resolved_infile)
        {
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                       "Failed to resolve '%s': '%s'", exec->infile, g_strerror (errno));
          goto bail;
        }

      g_free (exec->infile);
      exec->infile = g_steal_pointer (&resolved_infile);

      sandbox_dir = g_getenv ("FLATPAK_SANDBOX_DIR");
      if (!sandbox_dir || *sandbox_dir != '/')
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Incorrect sandbox directory: '%s'", sandbox_dir ? sandbox_dir : "(null)");
          goto bail;
        }

      g_mkdir_with_parents (sandbox_dir, 0700);
      tmpl = g_build_filename (sandbox_dir, "gnome-desktop-thumbnailer-XXXXXX", NULL);
      exec->outdir = g_mkdtemp (tmpl);
      if (!exec->outdir)
        {
          int errsv = errno;

          g_clear_pointer (&tmpl, g_free);
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                       "Could not create temporary sandbox directory: %s",
                       g_strerror (errsv));
          goto bail;
        }

      exec->outfile = g_build_filename (exec->outdir, "gnome-desktop-thumbnailer.png", NULL);
    }
  else if (exec->sandbox == SANDBOX_TYPE_NONE)
    {
      int fd;
      g_autofree char *tmpname = NULL;

      fd = g_file_open_tmp (".gnome_desktop_thumbnail.XXXXXX", &tmpname, error);
      if (fd == -1)
        goto bail;
      close (fd);
      exec->outfile = g_steal_pointer (&tmpname);
    }
  else
    g_assert_not_reached ();

  return exec;

bail:
  script_exec_free (exec);
  return NULL;
}

static void
clean_gst_registry_dir (ScriptExec *exec)
{
  g_autoptr(GDir) dir = NULL;
  g_autofree char *gst_cache_dir = NULL;
  g_autoptr(GError) error = NULL;
  const char *name = NULL;

  if (!exec->has_gst_registry)
    return;

  gst_cache_dir = create_gst_cache_dir ();
  dir = g_dir_open (gst_cache_dir, 0, &error);
  if (!dir) {
    g_debug ("Failed to open GStreamer registry dir %s: %s",
             gst_cache_dir,
             error->message);
    return;
  }

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      g_autofree char *fullpath = NULL;

      if (g_strcmp0 (name, ".") == 0 ||
          g_strcmp0 (name, "..") == 0 ||
          g_strcmp0 (name, GST_REGISTRY_FILENAME) == 0)
        continue;

      fullpath = g_build_filename (gst_cache_dir, name, NULL);
      if (g_remove (fullpath) < 0)
        g_warning ("Failed to delete left-over thumbnailing '%s'", fullpath);
      else
        g_warning ("Left-over file '%s' in '%s', deleting",
                   name, gst_cache_dir);
    }
}

static void
print_script_debug (GStrv expanded_script)
{
  GString *out;
  guint i;

  out = g_string_new (NULL);

  for (i = 0; expanded_script[i]; i++)
    g_string_append_printf (out, "%s ", expanded_script[i]);
  g_string_append_printf (out, "\n");

  g_debug ("About to launch script: %s", out->str);
  g_string_free (out, TRUE);
}

/* Compatibility shim for older versions of GLib */
#if !GLIB_CHECK_VERSION (2, 70, 0)
#define g_spawn_check_wait_status(status,error) g_spawn_check_exit_status (status, error)
#endif

GBytes *
gnome_desktop_thumbnail_script_exec (const char  *cmd,
				     int          size,
				     const char  *uri,
				     GError     **error)
{
  g_autofree char *error_out = NULL;
  g_auto(GStrv) expanded_script = NULL;
  int exit_status;
  gboolean ret;
  GBytes *image = NULL;
  ScriptExec *exec;

  exec = script_exec_new (uri, error);
  if (!exec)
    goto out;
  expanded_script = expand_thumbnailing_cmd (cmd, exec, size, error);
  if (expanded_script == NULL)
    goto out;

  print_script_debug (expanded_script);

  /* NOTE: Replace the error_out argument with NULL, if you want to see
   * the output of the spawned command and its children */
  ret = g_spawn_sync (NULL, expanded_script, NULL, G_SPAWN_SEARCH_PATH,
		      child_setup, exec->fd_array, NULL, &error_out,
		      &exit_status, error);
  if (ret && g_spawn_check_wait_status (exit_status, error))
    {
      char *contents;
      gsize length;

      if (g_file_get_contents (exec->outfile, &contents, &length, error))
        image = g_bytes_new_take (contents, length);
    }
  else
    {
      g_debug ("Failed to launch script: %s", !ret ? (*error)->message : error_out);
    }

  clean_gst_registry_dir (exec);

out:
  script_exec_free (exec);
  return image;
}

