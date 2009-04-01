
INCLUDES = \
	-I$(top_builddir)/include -I$(top_srcdir)/include \
	$(LIBNL1_CFLAGS) \
	$(I2400M_INCLUDES)

noinst_HEADERS = debug.h internal.h

libwimaxll_sources = 		\
	genl.c			\
	log.c			\
	misc.c			\
	op-open.c		\
        op-msg.c		\
        op-reset.c		\
        op-rfkill.c		\
        re-state-change.c	\
	wimax.c

libwimaxll_la_SOURCES = $(libwimaxll_sources)
# Trick automake
libwimaxll_la_CFLAGS = $(AM_CFLAGS)
# -version-info is CURRENT:REVISION:AGE
# CURRENT: inc for added, removed/changed interfaces
# REVISION: inc for changes that do not affect the external interface
# AGE: inc for added interfaces
#      set to zero if removed existing interfaces
libwimaxll_la_LDFLAGS = -version-info 1:0:1 $(LIBNL1_LIBS)
libwimaxll_a_SOURCES = $(libwimaxll_sources)

libwimaxll_i2400m_sources = 	\
	i2400m.c

libwimaxll_i2400m_la_SOURCES = $(libwimaxll_i2400m_sources)
# Trick automake
libwimaxll_i2400m_la_CFLAGS = $(AM_CFLAGS)
# -version-info is CURRENT:REVISION:AGE
# CURRENT: inc for added, removed/changed interfaces
# REVISION: inc for changes that do not affect the external interface
# AGE: inc for added interfaces
#      set to zero if removed existing interfaces
libwimaxll_i2400m_la_LDFLAGS = -lpthread -version-info 1:0:0 $(LIBNL1_LIBS)
libwimaxll_i2400m_a_SOURCES = $(libwimaxll_sources)

lib_LTLIBRARIES = libwimaxll.la libwimaxll-i2400m.la
lib_LIBRARIES = libwimaxll.a libwimaxll-i2400m.a

BUILT_SOURCES = names-vals.h

enum_names = \
	wimax_st

CLEANFILES = names-vals.h

names-vals.h: @LINUX_INCLUDE_PATH@/linux/wimax.h enum-to-names-vals
	rm -f $@
	echo "#include <linux/wimax.h>" > $@
	for type in $(enum_names); \
	do \
		$(srcdir)/enum-to-names-vals $$type $^; \
	done  >> $@

noinst_SCRIPTS = enum-to-names-vals

EXTRA_DIST = $(noinst_SCRIPTS)
