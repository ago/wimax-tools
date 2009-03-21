

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	$(I2400M_INCLUDES)

LDADD = ../lib/libwimaxll.la $(LIBNL1_LIBS)

bin_SCRIPTS = wimax-tools-version

bin_PROGRAMS = 				\
	wimaxll				\
	wimaxll-wait-for-state-change	\
	wimaxll-rfkill			\
	wimaxll-reset

AM_CFLAGS = @GLIB_CFLAGS@ -DPLUGINDIR=\""$(plugindir)"\"
AM_LDFLAGS = @GLIB_LIBS@ ../lib/libwimaxll.la $(LIBNL1_LIBS)

wimaxll_LDFLAGS = $(AM_LDFLAGS) -ldl -Wl,--export-dynamic -Wl,--version-script=wimaxll.ver
wimaxll_DEPENDENCIES = wimaxll.ver

wimaxll.exp: $(wimaxll_OBJECTS)
	nm -B *.o | awk '{ print $$3 }' | sort -u | grep -E -e '^w_' > $@

wimaxll.ver: wimaxll.exp
	echo "{ global:" > $@
	cat $< | sed -e "s/\(.*\)/\1;/" >> $@
	echo "local: *; };" >> $@

plugindir = $(pkglibdir)/plugins
plugin_LTLIBRARIES =		\
	wimaxll-pl-reset.la	\
	wimaxll-pl-rfkill.la

plugin_LDFLAGS = -no-undefined -module -avoid-version \
	-export-symbols-regex plugin

wimaxll_pl_reset_la_LDFLAGS = $(plugin_LDFLAGS)
wimaxll_pl_rfkill_la_LDFLAGS = $(plugin_LDFLAGS)
