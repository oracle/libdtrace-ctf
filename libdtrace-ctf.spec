# spec file for package libdtrace-ctf
#
# Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Licensed under the GNU General Public License (GPL), version 2. See the file
# COPYING in the top level of this tree.

BuildRequires: rpm
Name:         libdtrace-ctf
License:      GPLv2
Group:        Development/Libraries
Requires:     gcc elfutils-libelf zlib glib2
BuildRequires: elfutils-libelf-devel kernel-headers glibc-headers glib2-devel zlib-devel
Summary:      Compact Type Format library.
Version:      1.1.0
Release:      1%{?dist}
Source:       libdtrace-ctf-%{version}.tar.bz2
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
ExclusiveArch:    x86_64 sparc64 aarch64

%description
The Compact Type Format library provides a C-level representation of
a subset of the C type system.

Maintainers:
-----------
DTrace external development mailing list <dtrace-devel@oss.oracle.com>

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
%{_bindir}/ctf_ar
%defattr(-,root,root,755)
%exclude /usr/src/debug
%exclude /usr/lib/debug
%{_libdir}/libdtrace-ctf.so
%{_includedir}/sys/ctf.h
%{_includedir}/sys/ctf_api.h

%changelog
* Fri Dec 14 2018 - nick.alcock@oracle.com - 1.1.0-1
- Add ctf_add_{struct,union}_sized(). [Orabug: 29054972]
- Work around some very minor CTF-generation bugs seen in the wild
  in old UEK kernels. [Orabug: 28952429]
- Do not mprotect() the heap by mistake when reading CTF from a
  user-provided buffer. [Orabug: 28952429]
* Thu Nov 22 2018 - nick.alcock@oracle.com - 1.0.1-0.2
- Work around some CTF-generation bugs, narrower hack.
* Wed Oct 24 2018 - nick.alcock@oracle.com - 1.0.0-1
- Format v2, supporting many more types and enum/struct/union members.
v1 CTF files are transparently updated to v2. No soname change, but
some API for users directly accessing CTF files is broken.
[Orabug: 28150489]
* Fri May 04 2018 - nick.alcock@oracle.com - 0.8.1-1
- Fix ctf_rollback() in client containers to delete only the types
added since the last snapshot, rather than all of them.
[Orabug: 27971037]
* Mon Jan 29 2018 - nick.alcock@oracle.com - 0.8.0-1
- Add CTF_CHAR.
* Mon Jan 22 2018 - nick.alcock@oracle.com - 0.7.1-1
- Fix CTF archive alignment and failed write() handling
  (Tomas Jedlicka) [Orabug: 27191792, 27204447]
- Build on arm64 (Vincent Lim) [Orabug: 27418554]
* Tue Sep 12 2017 - nick.alcock@oracle.com - 0.7.0-1
- CTF archive support [Orabug: 25815388]
* Tue May 23 2017 - nick.alcock@oracle.com - 0.6.0-1
- Bitfield support (Robert M. Harris) [Orabug: 25815088]
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
* Thu Aug 30 2012 - nick.alcock@oracle.com - 0.3.0
- Split off from dtrace.
