# spec file for package dtrace-utils.
#
# Copyright 2011, 2012 Oracle, Inc.
#
# Licensed under the GNU General Public License (GPL), version 2. See the file
# COPYING in the top level of this tree.

BuildRequires: rpm
Name:         libdtrace-ctf
License:      GPLv2
Group:        Development/Libraries
Provides:     libdtrace-ctf
Requires:     gcc elfutils-libelf zlib
BuildRequires: elfutils-libelf-devel kernel-headers glibc-headers fakeroot zlib-devel
Summary:      Compact Type Format library.
Version:      0.3.0
Release:      1
Source:       libdtrace-ctf-%{version}.tar.bz2
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
ExclusiveArch:    x86_64

%description
The libdtrace-ctf package provides a DSO implementing the Compact Type Format
library, a C-level representation of a subset of the C type system.

Maintainers:
-----------
Nick Alcock <nick.alcock@oracle.com>
Kris van Hees <kris.van.hees@oracle.com>

%prep
%setup -q

%build
make -j $(getconf _NPROCESSORS_ONLN) VERSION=%{version}

%install
echo rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/sbin
fakeroot make DESTDIR=$RPM_BUILD_ROOT VERSION=%{version} install

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
/usr/lib/libdtrace-ctf.so*
/usr/include/sys/ctf.h
/usr/include/sys/ctf_api.h
/usr/include/sys/ctf_types.h

%changelog
* Thu May 17 2012 - nick.alcock@oracle.com - 0.3
- Split off from dtrace.
