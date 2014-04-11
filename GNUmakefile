# Top-level makefile for libdtrace-ctf.
#
# Build files in subdirectories are included by this file.
#
# Copyright 2011, 2012 Oracle, Inc.
#
# Licensed under the GNU General Public License (GPL), version 2. See the file
# COPYING in the top level of this tree.

.DELETE_ON_ERROR:
.SUFFIXES:

PROJECT := libdtrace-ctf
VERSION := 0.4.0

# Verify supported hardware.

$(if $(subst "x86_64",,$(shell uname -m)),,$(error "Error: DTrace for Linux only supports x86_64"),)
$(if $(subst "Linux",,$(shell uname -s)),,$(error "Error: DTrace only supports Linux"),)

CFLAGS ?= -O2 -g -Wall -pedantic -Wno-unknown-pragmas
LDFLAGS ?=
INVARIANT_CFLAGS := -std=gnu99 -D_LITTLE_ENDIAN -D_GNU_SOURCE $(DTO)
CPPFLAGS += -Iinclude -I$(objdir)
CC = gcc
override CFLAGS += $(INVARIANT_CFLAGS)
PREPROCESS = $(CC) -E -C

prefix = /usr
objdir := build-$(shell uname -r)
LIBDIR := $(DESTDIR)$(prefix)/lib64
BINDIR := $(DESTDIR)$(prefix)/bin
INCLUDEDIR := $(DESTDIR)$(prefix)/include
SBINDIR := $(DESTDIR)$(prefix)/sbin
DOCDIR := $(DESTDIR)$(prefix)/share/doc/libdtrace-ctf-$(VERSION)
TARGETS =

all::

$(shell mkdir -p $(objdir))

include Makeoptions
include Makefunctions
include Makeconfig
include Build $(wildcard $(sort */Build))
-include $(objdir)/*.d
include Makerules

all:: $(TARGETS)

clean::
	$(call describe-target,CLEAN,$(objdir))
	-rm -rf $(objdir)

realclean: clean
	-rm -f TAGS tags GTAGS GRTAGS GPATH

TAGS:
	$(call describe-target,TAGS)
	rm -f TAGS; find . -name '*.[ch]' | xargs etags -a

tags:
	$(call describe-target,tags)
	rm -f tags; find . -name '*.[ch]' | xargs ctags -a

gtags:
	$(call describe-target,gtags)
	gtags -i

PHONIES += all clean TAGS tags gtags

.PHONY: $(PHONIES)
