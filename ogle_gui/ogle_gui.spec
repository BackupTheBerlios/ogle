Summary: A GNOME interface for the ogle DVD player.
Name: ogle_gui
Version: 0.8.5
Release: ogle1
Vendor: Ogle
Packager: Ogle developer team
URL: http://www.dtek.chalmers.se/~dvd/
License: GPL
Group: Applications/Multimedia
Source: http://www.dtek.chalmers.se/~dvd/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
Requires: ogle >= 0.8.5, gtk+ >= 1.2.0, libxml2, libglade, libglade.so.0
BuildRequires: ogle-devel >= 0.8.5, gtk+-devel >= 1.2.0, libxml2-devel >= 2.4.19, libglade-devel

%description
This is a GNOME interface for the ogle DVD player.  Install this if you want
a more graphical gui than the one that comes by default with ogle.

%prep
%setup -q

%build
%configure
make

%install
test "${RPM_BUILD_ROOT}" != "/" && rm -rf ${RPM_BUILD_ROOT}
%makeinstall

# GNOME desktop entry
mkdir -p %{buildroot}%{_datadir}/gnome/apps/Multimedia
cat > %{buildroot}%{_datadir}/gnome/apps/Multimedia/ogle.desktop << EOF
[Desktop Entry]
Name=DVD Player (Ogle)
Comment=%{summary}
Exec=%{_bindir}/ogle
Icon=
Terminal=0
Type=Application
EOF

%clean
test "${RPM_BUILD_ROOT}" != "/" && rm -rf ${RPM_BUILD_ROOT}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)
%doc AUTHORS COPYING NEWS README
%{_prefix}/lib/ogle/*
%{_datadir}/locale/*/*/*
%{_datadir}/ogle_gui
%{_datadir}/gnome/apps/Multimedia/ogle.desktop

%changelog
* Mon Aug 5 2002 Björn Englund <d4bjorn@dtek.chalmers.se>
- Updated version to 0.8.5

* Sun Jun 30 2002 Björn Englund <d4bjorn@dtek.chalmers.se>
- Updated version to 0.8.4

* Mon Jun 10 2002 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.8.3

* Fri Dec 7 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- Updated version to 0.8.2
- Incorporated into cvs

* Sat Nov 24 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Rebuild against the current CVS Ogle version (new libdvdcontrol).

* Sun Nov  4 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Removed the execution of automake that would have the package depend on
  gcc3 if it was installed at build time.

* Thu Nov  1 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Fix for a stupid typo in the desktop entry.

* Wed Oct 31 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Spec file cleanup and fixes (Requires: and %files)
- Addedd a GNOME desktop entry.

* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- New version, added patch to install pixmaps correctly

* Thu Sep 20 2001 Martin Norbäck <d95mback@dtek.chalmers.se>
- initial version

