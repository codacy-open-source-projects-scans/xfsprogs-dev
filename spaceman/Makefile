# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2012 Red Hat, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = xfs_spaceman
HFILES = \
	init.h \
	space.h
CFILES = \
	file.c \
	health.c \
	info.c \
	init.c \
	prealloc.c \
	trim.c
LSRCFILES = xfs_info.sh

LLDLIBS = $(LIBHANDLE) $(LIBXCMD) $(LIBFROG)
LTDEPENDENCIES = $(LIBHANDLE) $(LIBXCMD) $(LIBFROG)
LLDFLAGS = -static

ifeq ($(ENABLE_EDITLINE),yes)
LLDLIBS += $(LIBEDITLINE) $(LIBTERMCAP)
endif

ifeq ($(HAVE_GETFSMAP),yes)
CFILES += freesp.c
endif

default: depend $(LTCOMMAND)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_SBIN_DIR)
	$(LTINSTALL) -m 755 $(LTCOMMAND) $(PKG_SBIN_DIR)
	$(INSTALL) -m 755 xfs_info.sh $(PKG_SBIN_DIR)/xfs_info
install-dev:

-include .dep
