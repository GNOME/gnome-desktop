# Note that this is NOT a relocatable package
%define ver      0.30
%define rel      SNAP
%define prefix   /usr

Summary: GNOME core programs
Name: gnome-core
Version: %ver
Release: %rel
Copyright: LGPL
Group: X11/Libraries
Source: ftp://ftp.gnome.org/pub/gnome-core-%{ver}.tar.gz
BuildRoot: /var/tmp/gnome-core-root
Obsoletes: gnome
Packager: Marc Ewing <marc@redhat.com>
URL: http://www.gnome.org
Prereq: /sbin/install-info
Docdir: %{prefix}/doc
Requires: xscreensaver
Requires: gnome-libs >= 0.30
Requires: ORBit >= 0.3.0
Summary(es): GNOME paquete de base
Summary(fr): GNOME paquetage de base

%description
Basic programs and libraries that are virtually required for
any GNOME installation.

GNOME is the GNU Network Object Model Environment.  That's a fancy
name but really GNOME is a nice GUI desktop environment.  It makes
using your computer easy, powerful, and easy to configure.

%description -l es
Programas y bibliotecas de base para el entorno GNOME.

GNOME (GNU Network Object Model Environment) es un entorno gráfico
orientado escritorio. Con él el uso de su computadora es más fácil,
agradable y eficaz.

%description -l fr
Programmes et bibliothèques de base pour l'environnent GNOME.

GNOME (GNU Network Object Model Environment) est un environnement graphique
de type bureau. Il rends l'utilisation de votre ordinateur plus facile,
agréable et eficace, et est facile à configurer.

%package devel
Summary: GNOME core libraries, includes, etc
Group: X11/Libraries
Requires: gnome-core
PreReq: /sbin/install-info
Summary(es): bibliotecas, includes, etc de la base de GNOME
Summary(fr): bibliothèques, en-têtes, etc pour la base de GNOME

%description devel
Panel libraries and header files.

%description devel -l es
Bibliotecas y include para el Panel.

%description devel -l fr
Bibliothèques et fichiers d'en-tête pour le Panel.

%changelog

* Sat Nov 21 1998 Pablo Saratxaga <srtxg@chanae.alphanet.ch>

- Cleaned %files section
- added spanish and french translations for rpm

* Wed Sep 23 1998 Michael Fulbright <msf@redhat.com>

- Built 0.30 release

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
%{prefix}/share/applets
%{prefix}/share/apps
%{prefix}/share/gnome
%{prefix}/share/idl
%{prefix}/share/locale/*/*/*
%{prefix}/share/pixmaps/*
%{prefix}/etc/*
%config %{prefix}/share/panelrc
%config %{prefix}/share/default.session

%files devel
%defattr(-, root, root)

%{prefix}/lib/*.sh
%{prefix}/lib/lib*.so
%{prefix}/lib/*a
%{prefix}/include/*
