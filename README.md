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

How to report bugs
==================

Bugs should be reported to the [Issues section of gnome-desktop repository](https://gitlab.gnome.org/GNOME/gnome-desktop/-/issues).

Please read the HACKING file for information on where to send changes or
bugfixes for this package.
