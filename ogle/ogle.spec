Summary: A DVD player for linux that supports DVD menus.
Name: ogle
Version: 0.8.2
Release: ogle1
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
Copyright: GPL
Group: Applications/Multimedia
Source: http://www.dtek.chalmers.se/~dvd/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
Requires: libdvdread >= 0.9.2, libdvdcss >= 0.0.3, libjpeg, libxml2 >= 2.4.5
BuildRequires: libdvdread-devel >= 0.9.2, libdvdcss-devel >= 0.0.3, libjpeg-devel, a52dec-devel, libxml2-devel >= 2.4.5
ExclusiveArch: i686 i586 ppc

%description
Ogle is a DVD player. It's features are: Supports DVD menus and navigation,
reads encrypted and unencrypted DVDs using libdvdread/libdvdcss, normal X11
and XFree86 Xvideo display support with subpicture overlay, audio and
subpicture selection, advanced subpicture commands such as fade/scroll and
wipe, detects and uses correct aspect for movie and menus, playing AC3 via
S/PDIF with an external command, fullscreen mode, screenshots with and
without subpicture overlay...


%package devel
Summary: Header files and static libraries from the Ogle DVD player.
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
These are the header files and static libraries from Ogle that are needed
to build programs that use it (like GUIs).

%prep
%setup -q

%build
%configure
make

%install
test "${RPM_BUILD_ROOT}" != "/" && rm -rf ${RPM_BUILD_ROOT}
# hack for library dependencies
LIBRARY_PATH=%{buildroot}/usr/lib/ogle
export LIBRARY_PATH
#%makeinstall
make DESTDIR=%{buildroot} install

%clean
test "${RPM_BUILD_ROOT}" != "/" && rm -rf ${RPM_BUILD_ROOT}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README
%{_bindir}/*
%dir %{_datadir}/ogle
%config %{_datadir}/ogle/oglerc
%{_datadir}/ogle/_conf.dtd
%dir %{_libdir}/ogle
%{_libdir}/ogle/*.so.*
%{_libdir}/ogle/ogle_*
%{_mandir}/man*/*

%files devel
%{_includedir}/*
%{_libdir}/ogle/*.so
%{_libdir}/ogle/*.la
%{_libdir}/ogle/*.a

%changelog
* Fri Dec 7 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.8.2
- Incorporated into cvs
- Removed ugly hack, it's not correct, the .la-file contains false
  information

* Sun Nov  4 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Added an ugly hack to prevent the need to build the package twice to
  have it work done correctly.

* Wed Oct 31 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Spec file cleanup and fixes.

* Fri Oct 26 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.8.1
* Thu Oct 11 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.8.0
* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Added missing .la files
* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.7.5
- Split into normal and devel package
* Thu Sep 6 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.7.4
* Fri Aug 10 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.7.2
* Sun Jul 22 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- initial version
