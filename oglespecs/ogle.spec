Summary: A DVD player for linux that supports DVD menus.
Name: ogle
Version: 0.7.5
Release: 2
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
Copyright: GPL
Group: Applications/Multimedia
Source: http://www.dtek.chalmers.se/~dvd/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}

%description
Ogle is a DVD player for linux (and solaris). It's features are:
* Supports DVD menus and navigation.
* Reads from mounted, unmounted DVDs and hard drive.
* Reads encrypted and unencrypted DVDs using libdvdread/libdvdcss.
* A new MPEG2 decoder with MMX and SUN Solaris mediaLib acceleration.
* Normal X11 and XFree86 Xvideo display support with subpicture overlay.
* Accelerated display on Sun FFB2 cards.
* Audio and subpicture selection.
* Handles advanced subpicture commands such as fade/scroll and wipe.
* Detects and uses correct aspect for movie and menus.
* Possible to play AC3 via S/PDIF with an external command.
* Hardware yuv2rgb on Sun FFB2(+) (Creator3D).
* Fullscreen mode.
* Screenshots with and without subpicture overlay.

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
%doc AUTHORS COPYING NEWS README
%{_bindir}/*
%{_libdir}/ogle/*.so.*
%{_mandir}/*

%files devel
%{_includedir}/*
%{_libdir}/ogle/*.so
%{_libdir}/ogle/*.la
%{_libdir}/ogle/*.a

%changelog
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
