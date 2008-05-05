#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="GNOME Core Desktop"

(test -f $srcdir/configure.in \
  && test -f $srcdir/HACKING \
  && test -d $srcdir/libgnome-desktop) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome directory"
    exit 1
}


which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME SVN"
    exit 1
}

if test "`which gvfs-copy`" != ""; then
	gvfs-copy http://api.gnome.org/gnome-about/foundation-members gnome-about/foundation-members.list
fi
if test ! -f gnome-about/foundation-members.list; then
	touch gnome-about/foundation-members.list
fi

REQUIRED_AUTOMAKE_VERSION=1.9 . gnome-autogen.sh
