#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="gnome-desktop"
REQUIRED_AUTOMAKE_VERSION=1.9
REQUIRED_M4MACROS=

(test -f $srcdir/configure.ac \
  && test -f $srcdir/$PKG_NAME.doap) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which gnome-autogen.sh || {
    echo "You need to install gnome-common."
    exit 1
}

# Fetch submodules if needed
if test ! -f libgnome-desktop/libgsystem/README; then
  echo "+ Setting up submodules"
  git submodule init
fi
git submodule update

. gnome-autogen.sh
