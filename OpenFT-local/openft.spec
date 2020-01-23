%define prefix     /usr
%define sysconfdir /etc

%define  RELEASE 1

Summary: OpenFT plugin for giFT
Name: gift-openft
Version: 0.2.1.5
Release: 1
Copyright: GPL
Group: Applications/Internet
URL: http://gift.sourceforge.net/
Source0: %{name}-%{version}.tar.gz
Buildroot: %_tmppath/%{name}-%{version}-root
Prefix: %{_prefix}
Requires: libgift >= 0.11.4 , libgiftproto >= 0.11.4

%description
Use this package to connect to OpenFT networks using giFT

%prep
%setup -q

%build
./configure
%__make

%install
make install DESTDIR=$RPM_BUILD_ROOT/

%clean
make clean

%files
%defattr(-,root,root)
%_libdir/giFT/*
%_datadir/giFT/*

%changelog
* Fri Sep 07 2003 Franco Catrin L. <fcatrin@tuxpan.com>
- updated to official package distribution
