gnome-desktop
=============

gnome-desktop contains the libgnome-desktop library as well as a data
file that exports the "GNOME" version to the Settings Details panel.

The libgnome-desktop library provides API shared by several applications
on the desktop, but that cannot live in the platform for various
reasons. There is no API or ABI guarantee, although we are doing our
best to provide stability. Documentation for the API is available with
gtk-doc.

You may download updates to the package from [download.gnome.org](https://download.gnome.org/sources/gnome-desktop/).

To discuss gnome-desktop, you may use the Platform group of [GNOME's
Discourse instance](https://discourse.gnome.org/c/platform/5).

Installation
============

gnome-desktop uses [meson](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project) to build its sources.

[Bubblewrap](https://github.com/containers/bubblewrap), installed as the
bwrap binary, is a non-optional dependency on platforms where it is
supported and thumbnailing will silently fail when it is not installed
at runtime.

[flatpak-xdg-utils](https://github.com/flatpak/flatpak-xdg-utils/)'s `flatpak-spawn`
helper is a non-optional dependency for using gnome-desktop's thumbnailer
when the app is being run within Flatpak >= 1.5.1.

How to report bugs
==================

Bugs should be reported to the [Issues section of gnome-desktop repository](https://gitlab.gnome.org/GNOME/gnome-desktop/-/issues).

Thumbnailing sandboxing
=======================

The thumbnailer sandboxing was built to prevent a number of different
potential attack vectors.

- The attacker wants to steal arbitrary secrets from your machine (a
  confidentiality failure), or overwrite arbitrary files (an integrity
  failure).
- The attacker is assumed to be capable of inducing you to download a
  crafted thumbnailable object (picture, video, ROM) that will crash a
  thumbnailer and get arbitrary code execution.
- Stealing your secrets is prevented by:
  - only giving the thumbnailer access to the file it's thumbnailing,
    plus public files from `/usr`-equivalent places, so that it can't
    leak the content of a secret file into the thumbnail of a less-secret
    file.
  - not giving it internet access, so that it can't upload the file it's
    thumbnailing to Wikileaks.
- Overwriting arbitrary files is prevented by making the output of the
  thumbnailer the only thing that can be written from inside the sandbox.
- Subverting other programs to do one of those is (hopefully) prevented by only
  allowing it to output PNG thumbnails, because we hope PNG reader libraries are
  a lot more secure than libraries to read exotic image formats.
