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

Dual License: GPL and UPL (Universal Permissive License) https://oss.oracle.com/licenses/upl/

## Dependencies

Dependencies include:
- elfutils
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

## Pull Requests and Support

We currently do not accept pull requests via GitHub, please contact us via the mailing list above.

The source code for libdtrace-ctf is published here without support. Compiled binaries are provided as part of Oracle Linux,
which is [free to download](http://www.oracle.com/technetwork/server-storage/linux/downloads/index.html), distribute and use.
Support for libdtrace-ctf is included in Oracle Linux support subscriptions. Individual packages and updates are available on the [Oracle Linux yum server](https://yum.oracle.com).
