Summary: A library for reading DVD-Video images.
Name: libdvdread
Version: 0.9.1
Release: 2
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
Copyright: GPL
Group: System Environment/Libraries
Source: http://www.dtek.chalmers.se/~dvd/libdvdread-%{version}.tar.gz
Patch: soname.diff
BuildRoot: %{_tmppath}/%{name}-%{version}

%description
libdvdread provides a simple foundation for reading DVD-Video images.

%package devel
Summary: libdvdread development package
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
livdvdread-devel includes development files for libdvdread

%prep
%setup -q
%patch -p0

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
%doc AUTHORS COPYING NEWS README 
%{_libdir}/*.so.*

%files devel
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/*.a
%{_libdir}/*.la

%changelog
* Tue Sep 25 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Added small patch to fix the ldopen of libdvdcss
* Tue Sep 18 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated to version 0.9.1
* Fri Sep 14 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Split into normal and devel package
* Thu Sep 6 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated to version 0.9.0
* Tue Jul 03 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- initial version
