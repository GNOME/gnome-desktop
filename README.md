gnome-desktop
=============

gnome-desktop contains the libgnome-desktop library as well as a data
file that exports the "GNOME" version to the Settings Details panel.

The libgnome-desktop library provides API shared by several applications
on the desktop, but that cannot live in the platform for various
reasons. There is no API or ABI guarantee, although we are doing our
best to provide stability. Documentation for the API is available with
gtk-doc.

You may download updates to the package from:

   http://download.gnome.org/sources/gnome-desktop/

To discuss gnome-desktop, you may use the desktop-devel-list mailing
list:

  http://mail.gnome.org/mailman/listinfo/desktop-devel-list


Installation
============

See the file 'INSTALL'. If you are not using a released version of
gnome-desktop (for example, if you checked out the code from git), you
first need to run './autogen.sh'.

Bubblewrap, installed as the bwrap binary, is a non-optional dependency
on platforms where it is supported and thumbnailing will silently fail
when it is not installed at runtime.

How to report bugs
==================

Bugs should be reported to the GNOME bug tracking system:

   https://bugzilla.gnome.org/ (product gnome-desktop)

You will need to create an account for yourself.

Please read the following page on how to prepare a useful bug report:

   https://bugzilla.gnome.org/page.cgi?id=bug-writing.html

Please read the HACKING file for information on where to send changes or
bugfixes for this package.
