

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	$(I2400M_INCLUDES)

LDADD = ../lib/libwimaxll.la $(LIBNL1_LIBS)

bin_SCRIPTS = wimax-tools-version

bin_PROGRAMS = 				\
	wimaxll-wait-for-state-change	\
	wimaxll-rfkill			\
	wimaxll-reset

