AC_PREREQ(2.50)
AC_INIT([WiMAX low level tools], [1.4.5],
        [linux-wimax@intel.com], [wimax-tools])
AC_CONFIG_SRCDIR([lib/internal.h])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(include/config.h)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE

AC_PREFIX_DEFAULT(/usr/local)

if (test "${CFLAGS}" = ""); then
	CFLAGS="-g -Wall -O2"
fi

AC_LANG_C
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_ARG_ENABLE(doxygen-doc,
              AC_HELP_STRING([--enable-doxygen-doc],
                             [Generate documentation with Doxygen]),
              doxygen_doc=$enableval,doxygen_doc=auto)

AC_PATH_PROG(DOXYGEN, doxygen, no)
AC_MSG_CHECKING([if Doxygen documentation should be built])
if test zz$DOXYGEN = zzno
then
    doxygen=no
else
    doxygen=yes
fi

case zz$doxygen_doc in
     zzauto)
     	do_doxygen=$doxygen;;
     zzyes)
        if test zz$doxygen = zzno
        then
                AC_MSG_ERROR([Doxygen docs forced on with --enable-doxygen-doc, but can't find doxygen!])
        fi
     	do_doxygen=yes;;
     zzno)
        do_doxygen=no;;
     *)
        AC_MSG_ERROR([Unknown enable val "$doxygen_doc" to --enable-doxygen-doc])
esac

AM_CONDITIONAL(DOXYGEN_DOC, test zz$do_doxygen = zzyes)
AC_MSG_RESULT($do_doxygen)


AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],
			[enable compiling with debugging information]), [
	if (test "${enableval}" = "yes" &&
				test "${ac_cv_prog_cc_g}" = "yes"); then
		CFLAGS="$CFLAGS -g"
	fi
])

# If libnl3 is installed
AC_ARG_WITH(libnl3,
	AC_HELP_STRING([--with-libnl3],
		[prefix to libnl3 installation; defaults to
		whichever is found in the system with pkg-config.
		USE ABSOLUTE PATHS.]
	),
	export PKG_CONFIG_PATH="$withval:$withval/lib/pkgconfig"
)

PKG_CHECK_MODULES([LIBNL3], [libnl-3.0, libnl-genl-3.0], [
	AC_DEFINE([HAVE_LIBNL3], [1], [Use libnl-3.0])
],[
	AC_MSG_ERROR([libnl-devel >= 3.0 is required])
])

AC_SUBST(LIBNL3_CFLAGS)
AC_SUBST(LIBNL3_LIBS)

# Linux WiMAX stack
LINUX_KERNEL=/usr
AC_ARG_WITH(linux,
            AC_HELP_STRING([--with-linux],
			   [path to the Linux kernel headers/source (containing the 
                            definitions for the WiMAX stack); defaults to what
                            is installed in your system's /usr/include/linux. 
                            USE ABSOLUTE PATHS.]),
            [
                LINUX_KERNEL="$withval"
            ])
# Old, for backwards compat
AC_ARG_WITH(i2400m,
            AC_HELP_STRING([--with-i2400m],
			   [path to the Linux kernel headers/source (containing the 
                            definitions for the WiMAX stack); defaults to what
                            is installed in your system's /usr/include/linux. 
                            DEPRECATED: this option will be removed soon.
                            USE ABSOLUTE PATHS.]),
            [
                LINUX_KERNEL="$withval"
            ])
AC_MSG_RESULT(Using Linux WiMAX stack from $LINUX_KERNEL)

AC_CHECK_FILE([$LINUX_KERNEL/include/linux/wimax.h],
                [AC_DEFINE([HAVE_WIMAX_H], 1, [Define to 1 if you have <linux/wimax.h>.])],
                [AC_MSG_ERROR([Can't locate include/linux/wimax.h in $LINUX_KERNEL])])

# Dirty (as in very dirty) version compatibility check
#
# To make sure it works during cross compiles, we extract by hand the
# value of WIMAX_GNL_VERSION.
wimax_h=${LINUX_KERNEL}/include/linux/wimax.h
wimax_gnl_version=[$(grep '^[[:space:]]*WIMAX_GNL_VERSION[[:space:]]*=[[:space:]]*[0-9]\+,$' $wimax_h \
     | sed 's/^[[:space:]]*WIMAX_GNL_VERSION[[:space:]]*=[[:space:]]*\([0-9]\+\),$/\1/')]
wimax_gnl_vmajor=$(($wimax_gnl_version / 10))
wimax_gnl_vminor=$(($wimax_gnl_version % 10))
need_vmajor=0
need_vminor=0
if test $wimax_gnl_vmajor != $need_vmajor
then
        AC_MSG_ERROR([WiMAX kernel support has major version $wimax_gnl_vmajor, this software needs $need_vmajor])
fi
if test $wimax_gnl_vminor -lt $need_vminor
then
        AC_MSG_ERROR([WiMAX kernel support has minor version $wimax_gnl_vminor, this software needs at least $need_vminor])
fi

AC_SUBST(I2400M_INCLUDES, "-I${LINUX_KERNEL}/include")
AC_SUBST(LINUX_INCLUDE_PATH, "${LINUX_KERNEL}/include")

AC_CHECK_DECL(WIMAX_GNL_OP_STATE_GET,
              AC_MSG_NOTICE([Kernel / WiMAX driver is ok (> 2.6.30); has WIMAX_GNL_OP_STATE_GET]),
              AC_MSG_ERROR([Kernel / WiMAX driver is too old (<= 2.6.30); missing WIMAX_GNL_OP_STATE_GET]), 
              [#include <$LINUX_INCLUDE_PATH/linux/wimax.h>])

AC_CHECK_LIB(dl, dlopen, dummy=yes,
			AC_MSG_ERROR(dynamic linking loader is required))

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14, dummy=yes,
                  AC_MSG_ERROR(GLib >= 2.14 is required))
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


AC_OUTPUT(Makefile
          wimax-tools.spec
          wimax-tools.yaml
          doc/Makefile
          doc/doxygen.conf
          libwimaxll-0.pc
          libwimaxll-0-uninstalled.pc
          libwimaxll-i2400m-0.pc
          libwimaxll-i2400m-0-uninstalled.pc
          wimaxll-cmd-0.pc
          bin/Makefile
          bin/wimax-tools-version
          include/Makefile
          include/wimaxll-version.h
          include/wimaxll/Makefile
          include/wimaxll/version.h
          lib/Makefile
          src/Makefile)
