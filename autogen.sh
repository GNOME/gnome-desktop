#!/bin/sh
# Run this to generate all the initial makefiles, etc.

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have "\`autoconf\'" installed to compile Gnome."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    DIE=1
}

(libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have "\`libtool\'" installed to compile Gnome."
    echo "Get ftp://alpha.gnu.org/gnu/libtool-1.0h.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have "\`automake\'" installed to compile Gnome."
    echo "Get ftp://ftp.cygnus.com/pub/home/tromey/automake-1.2d.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
    NO_AUTOMAKE=yes
}


# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: Missing "\`aclocal\'".  The version of "\`automake\'
    echo "installed doesn't appear recent enough."
    echo "Get ftp://ftp.cygnus.com/pub/home/tromey/automake-1.2d.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
}

if test "$DIE" -eq 1; then
    exit 1
fi

test -f /opt/lib/libgtk.a \
  || test -f /opt/gnome/lib/libgtk.a \
  || test -f /usr/lib/libgtk.a \
  || test -f /usr/local/lib/libgtk.a \
  || cat <<EOF
**Warning**: You must have Gtk installed to compile Gnome.  I cannot
find it installed in the usual places.  "configure" may do a better
job of finding out if you have it installed.  If Gtk is not installed,
visit ftp://ftp.gimp.org/pub/gtk/ (or get it out of CVS too).

EOF

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(test -f $srcdir/configure.in \
  && test -f $srcdir/HACKING \
  && test -d $srcdir/gsm) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome directory"
    exit 1
}

if test -z "$*"; then
    echo "**Warning**: I am going to run "\`configure\'" with no arguments."
    echo "If you wish to pass any to it, please specify them on the"
    echo \`$0\'" command line."
    echo
fi

for I in /usr /usr/local /opt /opt/gnome; do
	if [ -f $I/lib/gnomeConf.sh ]; then
		GNOMEINSTDIR="--with-gnome=$I"
	fi
done

for i in .
do 
    echo processing $srcdir/$i
    (cd $srcdir/$i; \
    libtoolize --copy --force; \
    if test -d macros; then aclocal -I macros; else aclocal; fi; \
    autoheader; \
    automake --add-missing; \
    automake --gnu; autoheader; autoconf)
done

echo running $srcdir/configure --enable-maintainer-mode $GNOMEINSTDIR "$@"
$srcdir/configure --enable-maintainer-mode $GNOMEINSTDIR "$@" \
&& echo Now type \`make\' to compile the Gnome Libraries.
