
INCLUDES = \
	-I$(top_builddir)/include -I$(top_srcdir)/include \
	$(I2400M_INCLUDES)

LDADD = ../lib/libwimaxll.la $(LIBNL3_LIBS)

testdir = $(pkglibdir)/test

test_PROGRAMS =			\
	test-dump-pipe		\
	test-rfkill
