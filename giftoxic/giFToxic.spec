%define name giFToxic
%define version 0.0.9
%define release 1

Summary: giFToxic GTK2 front-end for giFT
Name: %{name}
Version: %{version}
Release: %{release}
Source0: %{name}-%{version}.tar.gz
URL: http://giftoxic.sf.net/
Copyright: GPL
Group: Applications/Internet
BuildRoot: /var/tmp/%{name}-buildroot
Prefix: %{_prefix}
Provides: giFToxic
Requires: gtk2 >= 2.0.3, gettext >= 0.11.4, giFT >= 0.10.0 
BuildPrereq: gtk2-devel >= 2.0.3, gettext >= 0.11.4, giFT >= 0.10.0

%description
This is a GTK2 font end for giFT daemon (gift.sf.net).
Install this package if you want to use giFT from a GTK2/GNOME desktop.

%prep
rm -rf ${RPM_BUILD_ROOT}

%setup -q 

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} 
make

%install
rm -rf $RPM_BUILD_ROOT
make install-strip prefix=$RPM_BUILD_ROOT/%{prefix}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING AUTHORS ChangeLog README FAQ
%_bindir/*
%_datadir/*

%changelog
* Sun May 25 2003 Franco Catrin <fcatrin@tuxpan.cl>
- First spec file for RedHat distribution.
