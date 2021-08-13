dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([include/version.h])
AC_REVISION($Revision: 1.12 $)
AC_PREREQ(2.53)

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PATH_PROG(AR, ar)

if test -z "$AR" ; then
        AC_MSG_ERROR([*** 'ar' missing, please install or fix your \$PATH ***])
fi

if test -z "$LD" ; then
        LD=$CC
fi
AC_SUBST(LD)

AC_C_INLINE
if test "$GCC" = "yes" || test "$GCC" = "egcs"; then
        CFLAGS="$CFLAGS -Wall -Wpointer-arith -Wno-uninitialized"
fi

dnl Checks for libraries.
dnl Replace `main' with a function in -lpthread:
AC_CHECK_LIB(pthread, pthread_create)
dnl Replace `main' with a function in -lssl:
AC_CHECK_LIB(ssl, SSLeay )
dnl Replace `main' with a function in -lcrypto:
AC_CHECK_LIB(crypto, RAND_add )


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h sys/ioctl.h sys/time.h syslog.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(gethostname gettimeofday socket time dprintf)

# Checks for data types
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(short int, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long int, 4)
AC_CHECK_SIZEOF(long long int, 8)
AC_CHECK_SIZEOF(double, 8)

AC_OUTPUT(opcode/Makefile engine/Makefile transport/Makefile Makefile imagoc/Makefile assembler/Makefile memory/Makefile system/Makefile creator/Makefile builtins/Makefile msger/Makefile manpages/Makefile)

