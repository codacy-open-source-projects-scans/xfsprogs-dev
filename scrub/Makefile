# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2024 Oracle.  All Rights Reserved.
#

TOPDIR = ..
builddefs=$(TOPDIR)/include/builddefs
include $(builddefs)

SCRUB_PREREQS=$(HAVE_GETFSMAP)

scrub_svcname=xfs_scrub@.service

ifeq ($(SCRUB_PREREQS),yes)
LTCOMMAND = xfs_scrub
INSTALL_SCRUB = install-scrub
XFS_SCRUB_ALL_PROG = xfs_scrub_all
XFS_SCRUB_FAIL_PROG = xfs_scrub_fail
XFS_SCRUB_ARGS = -b -n
ifeq ($(HAVE_SYSTEMD),yes)
INSTALL_SCRUB += install-systemd
SYSTEMD_SERVICES = $(scrub_svcname) xfs_scrub_all.service xfs_scrub_all.timer xfs_scrub_fail@.service
OPTIONAL_TARGETS += $(SYSTEMD_SERVICES)
endif
ifeq ($(HAVE_CROND),yes)
INSTALL_SCRUB += install-crond
CRONTABS = xfs_scrub_all.cron
OPTIONAL_TARGETS += $(CRONTABS)
# Don't enable the crontab by default for now
CROND_DIR = $(PKG_DATA_DIR)
endif

endif	# scrub_prereqs

UDEV_RULES = xfs.rules
ifeq ($(HAVE_UDEV),yes)
	INSTALL_SCRUB += install-udev
endif

HFILES = \
common.h \
counter.h \
descr.h \
disk.h \
filemap.h \
fscounters.h \
inodes.h \
progress.h \
read_verify.h \
repair.h \
scrub.h \
spacemap.h \
unicrash.h \
vfs.h \
xfs_scrub.h

CFILES = \
common.c \
counter.c \
descr.c \
disk.c \
filemap.c \
fscounters.c \
inodes.c \
phase1.c \
phase2.c \
phase3.c \
phase4.c \
phase5.c \
phase6.c \
phase7.c \
progress.c \
read_verify.c \
repair.c \
scrub.c \
spacemap.c \
vfs.c \
xfs_scrub.c

LLDLIBS += $(LIBHANDLE) $(LIBFROG) $(LIBICU_LIBS) $(LIBRT) $(LIBURCU) \
	$(LIBPTHREAD)
LTDEPENDENCIES += $(LIBHANDLE) $(LIBFROG)
LLDFLAGS = -static

ifeq ($(HAVE_MALLINFO),yes)
LCFLAGS += -DHAVE_MALLINFO
endif

ifeq ($(HAVE_MALLINFO2),yes)
LCFLAGS += -DHAVE_MALLINFO2
endif

ifeq ($(HAVE_LIBATTR),yes)
LCFLAGS += -DHAVE_LIBATTR
endif

ifeq ($(HAVE_LIBICU),yes)
CFILES += unicrash.c
LCFLAGS += -DHAVE_LIBICU $(LIBICU_CFLAGS)
endif

LDIRT = $(XFS_SCRUB_ALL_PROG) $(XFS_SCRUB_FAIL_PROG) *.service *.cron

default: depend $(LTCOMMAND) $(XFS_SCRUB_ALL_PROG) $(XFS_SCRUB_FAIL_PROG) $(OPTIONAL_TARGETS)

xfs_scrub_all: xfs_scrub_all.in $(builddefs)
	@echo "    [SED]    $@"
	$(Q)$(SED) -e "s|@sbindir@|$(PKG_SBIN_DIR)|g" \
		   -e "s|@scrub_svcname@|$(scrub_svcname)|g" \
		   -e "s|@pkg_version@|$(PKG_VERSION)|g" \
		   -e "s|@scrub_args@|$(XFS_SCRUB_ARGS)|g" < $< > $@
	$(Q)chmod a+x $@

xfs_scrub_fail: xfs_scrub_fail.in $(builddefs)
	@echo "    [SED]    $@"
	$(Q)$(SED) -e "s|@sbindir@|$(PKG_SBIN_DIR)|g" \
		   -e "s|@scrub_svcname@|$(scrub_svcname)|g" \
		   -e "s|@pkg_version@|$(PKG_VERSION)|g"  < $< > $@
	$(Q)chmod a+x $@

phase5.o unicrash.o xfs.o: $(builddefs)

include $(BUILDRULES)

install: $(INSTALL_SCRUB)

%.service: %.service.in $(builddefs)
	@echo "    [SED]    $@"
	$(Q)$(SED) -e "s|@sbindir@|$(PKG_SBIN_DIR)|g" \
		   -e "s|@scrub_args@|$(XFS_SCRUB_ARGS)|g" \
		   -e "s|@pkg_libexec_dir@|$(PKG_LIBEXEC_DIR)|g" \
		   < $< > $@

%.cron: %.cron.in $(builddefs)
	@echo "    [SED]    $@"
	$(Q)$(SED) -e "s|@sbindir@|$(PKG_SBIN_DIR)|g" < $< > $@

install-systemd: default $(SYSTEMD_SERVICES)
	$(INSTALL) -m 755 -d $(SYSTEMD_SYSTEM_UNIT_DIR)
	$(INSTALL) -m 644 $(SYSTEMD_SERVICES) $(SYSTEMD_SYSTEM_UNIT_DIR)
	$(INSTALL) -m 755 -d $(PKG_LIBEXEC_DIR)
	$(INSTALL) -m 755 $(XFS_SCRUB_FAIL_PROG) $(PKG_LIBEXEC_DIR)

install-crond: default $(CRONTABS)
	$(INSTALL) -m 755 -d $(CROND_DIR)
	$(INSTALL) -m 644 $(CRONTABS) $(CROND_DIR)

install-scrub: default
	$(INSTALL) -m 755 -d $(PKG_SBIN_DIR)
	$(LTINSTALL) -m 755 $(LTCOMMAND) $(PKG_SBIN_DIR)
	$(INSTALL) -m 755 $(XFS_SCRUB_ALL_PROG) $(PKG_SBIN_DIR)

install-udev: $(UDEV_RULES)
	$(INSTALL) -m 755 -d $(UDEV_RULE_DIR)
	for i in $(UDEV_RULES); do \
		$(INSTALL) -m 644 $$i $(UDEV_RULE_DIR)/64-$$i; \
	done

install-dev:

-include .dep
