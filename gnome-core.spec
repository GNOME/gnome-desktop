# Note that this is NOT a relocatable package
%define ver      0.28
%define rel      SNAP
%define prefix   /usr

Summary: GNOME core programs
Name: gnome-core
Version: %ver
Release: %rel
Copyright: LGPL
Group: X11/Libraries
Source: ftp://ftp.gnome.org/pub/gnome-core-%{ver}.tar.gz
BuildRoot: /tmp/gnome-core-root
Obsoletes: gnome
Packager: Marc Ewing <marc@redhat.com>
URL: http://www.gnome.org
Prereq: /sbin/install-info
Docdir: %{prefix}/doc
Requires: xscreensaver

%description
Basic programs and libraries that are virtually required for
any GNOME installation.

GNOME is the GNU Network Object Model Environment.  That's a fancy
name but really GNOME is a nice GUI desktop environment.  It makes
using your computer easy, powerful, and easy to configure.

%package devel
Summary: GNOME core libraries, includes, etc
Group: X11/Libraries
Requires: gnome-core
PreReq: /sbin/install-info

%description devel
Panel libraries and header files.

%changelog

* Fri Mar 13 1998 Marc Ewing <marc@redhat.com>

- Integrate into gnome-core CVS source tree

%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%prefix
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/bin/*
%{prefix}/lib/lib*.so.*
%{prefix}/share/*

%files devel
%defattr(-, root, root)

%{prefix}/lib/lib*.so
%{prefix}/lib/*a
%{prefix}/include/*
