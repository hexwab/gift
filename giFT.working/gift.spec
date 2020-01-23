%define prefix     /usr
%define sysconfdir /etc

%define  RELEASE 1
%define  rel     %{?CUSTOM_RELEASE} %{!?CUSTOM_RELEASE:%RELEASE}

Summary: giFT: Internet File Transfer
Name: giftd
Version: 0.11.7
Release: %rel
Copyright: GPL
Group: Applications/Internet
URL: http://gift.sourceforge.net/
Source0: gift-%{version}.tar.gz
Buildroot: %_tmppath/%{name}-%{version}-root
Prefix: %{_prefix}
Requires: libgift = %{version}, libgiftproto = %{version}

%package -n libgift
Summary: Library for giFT based applications
Group: System Environment/Libraries

%package -n libgift-devel
Summary: Library for developing giFT based applications
Group: Development/Libraries
Requires: libgift = %{version}

%package -n libgiftproto
Summary: Library for giFT protocol implementation plugins
Group: System Environment/Libraries
Requires: libgift = %{version}

%package -n libgiftproto-devel
Summary: Library for developing protocol implementations plugins for giFT
Group: Development/Libraries
Requires: libgiftproto = %{version}, libgift-devel = %{version}


%description
giFT is a `recursive' acronym for `giFT: Internet File
Transfer'. The giFT project is actually a collection of several
components together: the giFT daemon (which acts as bridge to the
actual file sharing protocols), OpenFT (a p2p network designed to
exploit all the functionality giFT supports), and a user
interface front-end.

Note that giFT is alpha software, and is still very much under
development!  You should update often.

%description -n libgift
Library for giFT based applications, including the giftd daemon
and client applications

%description -n libgift-devel
Library for giFT based applications development, including the
giftd daemon and client applications

%description -n libgiftproto
Library required by giFT protocol implementation plugins.

%description -n libgiftproto-devel
Library for giFT protocol implementation plugins development

%prep
%setup -q -n gift-%{version}

%build
./configure --prefix=%{prefix}
%__make

%install
make install DESTDIR=$RPM_BUILD_ROOT/

%clean
make clean

%files
%defattr(-,root,root)
%_bindir/*
%{prefix}/man/man1/*
%_datadir/giFT/ui/*
%_datadir/giFT/giftd*
%_datadir/giFT/mime*

%files -n libgift
%defattr(-,root,root)
%_libdir/libgift.*

%files -n libgift-devel
%defattr(-,root,root)
%{prefix}/include/libgift/*.h
%_libdir/pkgconfig/libgift.*

%files -n libgiftproto
%defattr(-,root,root)
%_libdir/libgiftproto.*

%files -n libgiftproto-devel
%defattr(-,root,root)
%{prefix}/include/libgift/proto/*


%changelog
* Fri Sep 07 2003 Franco Catrin L. <fcatrin@tuxpan.com>
- updated to official package distribution
* Wed Apr 17 2002 Eelco Lempsink <eelco@33lc0.net>
- updated description (name change)
* Tue Feb 19 2002 James Ralston <ralston@pobox.com>
- created
