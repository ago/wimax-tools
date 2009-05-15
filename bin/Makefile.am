

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	$(I2400M_INCLUDES)

LDADD = ../lib/libwimaxll.la $(LIBNL1_LIBS)

bin_SCRIPTS = \
	wimax-tools-version		\
	wimaxll-reset			\
	wimaxll-rfkill			\
	wimaxll-wait-for-state-change

EXTRA_DIST = $(bin_SCRIPTS)

bin_PROGRAMS = 				\
	wimaxll

AM_CFLAGS = @GLIB_CFLAGS@ -DPLUGINDIR=\""$(plugindir)"\"
AM_LDFLAGS = @GLIB_LIBS@ ../lib/libwimaxll.la $(LIBNL1_LIBS)

wimaxll_LDFLAGS = $(AM_LDFLAGS) -ldl -Wl,--export-dynamic -Wl,--version-script=wimaxll.ver
wimaxll_DEPENDENCIES = wimaxll.ver

wimaxll.exp: $(wimaxll_OBJECTS)
	nm -B *.o | awk '{ print $$3 }' | sort -u | grep -E -e '^(w|wimaxll)_' > $@

wimaxll.ver: wimaxll.exp
	echo "{ global:" > $@
	cat $< | sed -e "s/\(.*\)/\1;/" >> $@
	echo "local: *; };" >> $@

CLEANFILES = wimaxll.exp wimaxll.ver

plugindir = $(pkglibdir)/plugins
plugin_LTLIBRARIES =		\
	wimaxll-pl-reset.la	\
	wimaxll-pl-rfkill.la	\
	wimaxll-pl-wfsc.la

plugin_LDFLAGS = -no-undefined -module -avoid-version \
	-export-symbols-regex plugin

wimaxll_pl_reset_la_LDFLAGS = $(plugin_LDFLAGS)
wimaxll_pl_rfkill_la_LDFLAGS = $(plugin_LDFLAGS)
wimaxll_pl_wfsc_la_LDFLAGS = $(plugin_LDFLAGS)
