%define  RELEASE 1

Summary: Gnutella plugin for giFT
Name: gift-gnutella
Version: 0.0.10
Release: 1
Copyright: GPL
Group: Applications/Internet
URL: http://gift.sourceforge.net/
Source0: %{name}-%{version}.tar.gz
Buildroot: %_tmppath/%{name}-%{version}-root
Prefix: %{_prefix}
Requires: libgift >= 0.11.4 , libgiftproto >= 0.11.4

%description
Use this package to connect to Gnutella networks using giFT

%prep
%setup -q

%build
%configure
%__make

%install
%makeinstall

%clean
make clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root)
%_libdir/giFT/*
%_datadir/giFT/*

%changelog
* Sat Feb 14 2004 Craig Barnes <hbelfaq@softhome.net>
- use %configure macro which should work properly now
- use %makeinstall macro
- remove build root when finished

* Sun Sep 07 2003 Franco Catrin L. <fcatrin@tuxpan.com>
- updated to official package distribution
