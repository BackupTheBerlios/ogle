Summary: A gui for the ogle DVD player
Name: ogle_gui
Version: 0.7.5
Release: 2
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
Copyright: GPL
Group: Applications/Multimedia
Source: http://www.dtek.chalmers.se/~dvd/%{name}-%{version}.tar.gz
Patch: pixmaps.diff
BuildRoot: %{_tmppath}/%{name}-%{version}

%description
ogle_gui is a gui for the ogle DVD player.
Install this if you want a more graphical gui than the one that comes
with ogle

%prep
%setup -q
%patch -p0

%build
automake
%configure --with-dvdcontrol=%{_prefix}
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
%{_datadir}/pixmaps

%changelog
* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- New version, added patch to install pixmaps correctly
* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- initial version
