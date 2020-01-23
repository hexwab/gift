%define prefix     /usr
%define sysconfdir /etc

%define  RELEASE 1
%define  rel     %{?CUSTOM_RELEASE} %{!?CUSTOM_RELEASE:%RELEASE}

Summary: The GNU Internet File Transfer project
Name: giFT
Version: 0.10.0
Release: %rel
Copyright: GPL
Group: Applications/Internet
URL: http://gift.sourceforge.net/
Source0: %{name}-%{version}.tar.gz
Source1: giFT.desktop
Buildroot: %_tmppath/%{name}-%{version}-root

%description
giFT is a term used to describe the GNU Internet File Transfer
project, which is actually a collection of several components
together: the giFT daemon (which acts as bridge to the actual file
sharing protocols), OpenFT (a p2p network designed to exploit all the
functionality giFT supports), and a user interface front-end.

Note that giFT is alpha software, and is still very much under
development!  You should update often.

%prep
%setup -q

%build
libtoolize --force --copy
aclocal
autoheader
automake --add-missing --gnu
autoconf
%configure
%__make

%install
[ -n "${RPM_BUILD_ROOT}" ] && %__rm -rf "${RPM_BUILD_ROOT}"
%__make "DESTDIR=${RPM_BUILD_ROOT}" install
%__install -d "${RPM_BUILD_ROOT}%_sysconfdir/X11/applnk/Internet"
%__install -m 0444 %SOURCE1 "${RPM_BUILD_ROOT}%_sysconfdir/X11/applnk/Internet/giFT.desktop"

%clean
[ -n "${RPM_BUILD_ROOT}" ] && %__rm -rf "${RPM_BUILD_ROOT}"
( cd "${RPM_BUILD_DIR}" && %__rm -rf "%{name}-%{version}" )

%files
%defattr(-,root,root)
%_sysconfdir/X11/applnk/Internet/*
%_bindir/*
%_libdir/*
%_datadir/*

%changelog
* Tue Feb 19 2002 James Ralston <ralston@pobox.com>
- created
