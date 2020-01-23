%define prefix     /usr

%define  RELEASE 1

Summary: FastTrack plugin for giFT
Name: giFT-FastTrack
Version: 0.8.9
Release: 1
Copyright: GPL
Group: Applications/Internet
URL: http://developer.berlios.de/projects/gift-fasttrack/
Source0: %{name}-%{version}.tar.gz
Buildroot: %_tmppath/%{name}-%{version}-root
Prefix: %{_prefix}
Requires: libgift >= 0.11.4 , libgiftproto >= 0.11.4

%description
Use this package to connect to FastTrack (KaZaa) networks using giFT

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
* Mon Sep 08 2003 Franco Catrin L. <fcatrin@tuxpan.com>
- updated to official package distribution
