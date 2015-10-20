# spec file for package libdtrace-ctf
#
# Copyright 2011, 2012, 2013, 2014, 2015 Oracle, Inc.
#
# Licensed under the GNU General Public License (GPL), version 2. See the file
# COPYING in the top level of this tree.

BuildRequires: rpm
Name:         libdtrace-ctf
License:      GPLv2
Group:        Development/Libraries
Requires:     gcc elfutils-libelf zlib
BuildRequires: elfutils-libelf-devel kernel-headers glibc-headers zlib-devel
Summary:      Compact Type Format library.
Version:      0.5.0
Release:      3%{?dist}
Source:       libdtrace-ctf-%{version}.tar.bz2
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
ExclusiveArch:    x86_64 sparc64

%description
The Compact Type Format library provides a C-level representation of
a subset of the C type system.

Maintainers:
-----------
Nick Alcock <nick.alcock@oracle.com>
Kris van Hees <kris.van.hees@oracle.com>

%package devel
Summary:      Compact Type Format development headers.
Requires:     %{name}%{?_isa} = %{version}-%{release}
Requires:     zlib-devel

%description devel
Headers and libraries to develop applications using the Compact Type Format.

%prep
%setup -q

%build
make -j $(getconf _NPROCESSORS_ONLN) VERSION=%{version}

%install
echo rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/sbin
make DESTDIR=$RPM_BUILD_ROOT VERSION=%{version} install

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"
rm -rf $RPM_BUILD_DIR/%{name}-%{version}

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root,755)
%exclude /usr/src/debug
%exclude /usr/lib/debug
%{_libdir}/libdtrace-ctf.so.*
%{_includedir}/sys/ctf_types.h

%files devel
%defattr(-,root,root,-)
%{_bindir}/ctf_dump
%defattr(-,root,root,755)
%exclude /usr/src/debug
%exclude /usr/lib/debug
%{_libdir}/libdtrace-ctf.so
%{_includedir}/sys/ctf.h
%{_includedir}/sys/ctf_api.h

%changelog
* Fri Aug 14 2015 - nick.alcock@oracle.com - 0.5.0-3
- Include the distribution in the RPM release. [Orabug: 21211461]
- No longer Provide: our own name. [Orabug: 21622263]
* Thu Apr 23 2015 - nick.alcock%oracle.com - 0.5.0-2
- libdtrace-ctf-devel now depends on the appropriate version of libdtrace-ctf.
[Orabug: 20948460]
* Fri Mar 20 2015 - nick.alcock@oracle.com - 0.5.0
- SPARC / big-endian support. [Orabug: 20762799]
* Tue Mar 17 2015 - nick.alcock@oracle.com - 0.4.3
- New ctf_snapshot() and ctf_rollback() functions. [Orabug: 20229533]
* Mon Oct 13 2014 - nick.alcock@oracle.com - 0.4.2
- Work with GNU Make 4.0.
* Tue Dec 17 2013 - nick.alcock@oracle.com - 0.4.1
- Improvements to ctf_dump.
- No longer look off the end of strings when looking up types by name.
* Tue Jul 23 2013 - nick.alcock@oracle.com - 0.4.0
- New ctf_dump tool and ctf_variable_iter() iteration function.
* Wed Nov 28 2012 - kris.van.hees@oracle.com - 0.3.3
- Report errors on type lookup correctly.
* Fri Nov  2 2012 - nick.alcock@oracle.com - 0.3.2
- CTF sections renamed to .ctf.
* Fri Aug 30 2012 - nick.alcock@oracle.com - 0.3.0
- Split off from dtrace.
