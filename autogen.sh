#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="gnome-desktop"
REQUIRED_AUTOMAKE_VERSION=1.9
REQUIRED_M4MACROS=

(test -f $srcdir/configure.in \
  && test -f $srcdir/$PKG_NAME.doap) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which gnome-autogen.sh || {
    echo "You need to install gnome-common."
    exit 1
}

if test "`which gvfs-copy`" != ""; then
	gvfs-copy http://api.gnome.org/gnome-about/foundation-members gnome-about/foundation-members.list
	gvfs-copy "http://git.fedorahosted.org/git/?p=hwdata.git;a=blob_plain;f=pnp.ids;hb=HEAD" libgnome-desktop/pnp.ids
fi
if test ! -f gnome-about/foundation-members.list; then
	touch gnome-about/foundation-members.list
fi
if test ! -f libgnome-desktop/pnp.ids; then
	touch libgnome-desktop/pnp.ids
fi

. gnome-autogen.sh
