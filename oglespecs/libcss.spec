Summary: A library for handling encrypted dvds.
Name: libcss
Version: 0.1.0
Release: 1
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
Copyright: GPL
Group: System Environment/Libraries
Source: http://www.linuxvideo.org/oms/data/libcss-0.1.0.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}

%description
Library for handling encrypted dvds.

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
%doc AUTHORS COPYING FAQ NEWS README
%{_includedir}/*.h
%{_libdir}/*

%changelog
* Sun Jul 22 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- initial version
