# NOTE

This project is superseded by the CTF support in GNU binutils: it is only
used by old versions of its dependent projects.  It is still published here
in case a bugfix is needed to support one of those projects.

The project has been merged into GNU binutils 2.33 and GCC 12; as of
binutils 2.36 it is much more capable and can deduplicate CTF more reliably
and rapidly than any other implementation (to my knowledge).  Please submit
changes to [binutils upstream](https://sourceware.org/binutils/).  (Please
indicate if you need those changes in an old project that still uses
libdtrace-ctf, but all changes should flow in through GNU binutils first.)

# Linux libdtrace-ctf

libdtrace-ctf is a port of the Solaris Compact Type Format library to Linux.
It's meant to be used with DTrace for Linux.
Its file format is similar to but not compatible with Solaris CTF files.
The public API is source-compatible, to ease adoption of Linux for libctf users.

The source has been available at
[oss.oracle.com](https://oss.oracle.com/git/gitweb.cgi?p=libdtrace-ctf.git;a=tags),
as a git repository with full git history.
By posting the source here on github.com, we hope to increase the visibility for our work
and to make it even easier for people to access the source.
We will also use this repository to work with developers in the Linux community.

See the [dtrace-utils README](https://github.com/oracle/dtrace-utils)
for more information on how to build DTrace utilities
with `libdtrace-ctf` for the Linux kernel.

## License

libdtrace-ctf is licensed under either the GPLv2+ or the UPL 1.0 (Universal
Permissive License). A copy of the GPLv2 license is included in this repository
as the COPYING file. A copy of the UPL 1.0 license is included in this repository
as the LICENSE.txt file.

## Dependencies

Dependencies may include:
- elfutils (to the extent that elfutils is utilized, libdtrace-ctf uses only
  those portions of elfutils that are licensed LGPLv3)
- zlib

## Build

Build as follows:

```
$ make
$ sudo make install
```

For more detail, see [README.build-system](README.build-system).

## Questions

For questions, check the
[dtrace-devel mailing list](https://oss.oracle.com/mailman/listinfo/dtrace-devel).

## Contributing

This project is not accepting external contributions at this time. For bugs or enhancement requests, please file a GitHub issue unless it’s security related. When filing a bug remember that the better written the bug is, the more likely it is to be fixed. If you think you’ve found a security vulnerability, do not raise a GitHub issue and follow the instructions in our [security policy](./SECURITY.md).

Please contact us via the mailing list above.

The source code for libdtrace-ctf is published here without support. Compiled binaries are provided as part of Oracle Linux,
which is [free to download](http://www.oracle.com/technetwork/server-storage/linux/downloads/index.html), distribute and use.
Support for libdtrace-ctf is included in Oracle Linux support subscriptions. Individual packages and updates are available on the [Oracle Linux yum server](https://yum.oracle.com).

## Security

Please consult the [security guide](./SECURITY.md) for our responsible security vulnerability disclosure process
