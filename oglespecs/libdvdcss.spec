Summary: A library for handling encrypted dvds.
Name: libdvdcss
Version: 0.0.3.ogle2
Release: 1
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
Copyright: GPL
Group: System Environment/Libraries
Source: http://www.dtek.chalmers.se/~dvd/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}

%description
Library for handling encrypted dvds.

%package devel
Summary: libdvdcss development package
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
libdvdcss-devel includes development files for libdvdcss

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
%doc AUTHORS ChangeLog COPYING README
%{_libdir}/*.so.*

%files devel
%{_includedir}/*
%{_libdir}/*.a
%{_libdir}/*.so

%changelog
* Tue Sep 18 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- New version 0.0.3.ogle2
* Fri Sep 14 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Split into normal and devel package
* Thu Sep 6 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- initial version
