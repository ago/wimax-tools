
INCLUDES = \
	-I$(top_builddir)/include -I$(top_srcdir)/include \
	$(LIBNL1_CFLAGS) \
	$(I2400M_INCLUDES)

noinst_HEADERS = debug.h internal.h

lib_LTLIBRARIES = libwimaxll.la
lib_LIBRARIES = libwimaxll.a
libwimaxll_sources = 		\
	op-open.c		\
        op-msg.c		\
        op-reset.c		\
        op-rfkill.c		\
        mc_rx.c			\
        pipe.c			\
        re-state-change.c	\
	wimax.c

libwimaxll_la_SOURCES = $(libwimaxll_sources)
# Trick automake
libwimaxll_la_CFLAGS = $(AM_CFLAGS)
# -version-info is CURRENT:REVISION:AGE
# REVISION: inc for changes that do not affect the external interface
# CURRENT: inc for added interfaces
# AGE: inc for removed/changed existing interfaces
libwimaxll_la_LDFLAGS = -version-info 0:0:0 $(LIBNL1_LIBS)
libwimaxll_a_SOURCES = $(libwimaxll_sources)

# Workaround renaming of libwimax to libwimaxll
#
# Dirty, but works the best -- will be removed soon
install-exec-local:
	$(mkdir_p) $(DESTDIR)/$(libdir)
	(cd $(DESTDIR)/$(libdir) && ln -sf libwimaxll.so.0 libwimax.so.0)
	(cd $(DESTDIR)/$(libdir) && ln -sf libwimaxll.so libwimax.so)
