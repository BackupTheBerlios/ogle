Summary: A library for handling encrypted dvds.
Name: a52dec
Version: 0.7.1b
Release: 2
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
Copyright: GPL
Group: Applications/Multimedia
Source: http://liba52.sourceforge.net/files/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}

%description
liba52 is a free library for decoding ATSC A/52 streams. It is released
under the terms of the GPL license. The A/52 standard is used in a
variety of applications, including digital television and DVD. It is
also known as AC-3.

%package devel
Summary: %{name} development package
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
%{name}-devel includes development files for %{name}

%prep
%setup -q

%build
%configure
make

%install
%makeinstall

%clean
[ "${RPM_BUILD_ROOT}" != "/" ] && rm -rf ${RPM_BUILD_ROOT}

%post
/sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc AUTHORS COPYING NEWS README TODO
%{_bindir}/*
%{_mandir}/*

%files devel
%{_includedir}/*
%{_libdir}/*.a
%{_libdir}/*.la

%changelog
* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Added missing .la files
- Building statically
* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Initial version
