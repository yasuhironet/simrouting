#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(simrouting, 0.2, yasu@sfc.wide.ad.jp)
AC_CONFIG_SRCDIR([simrouting.c])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])

#CFLAGS="$CFLAGS -O0 -g -pg -Wall";
CFLAGS="$CFLAGS -O0 -g -Wall";
#CFLAGS="$CFLAGS -O3 -Wall";

AC_ARG_WITH(netsnmp,
[  --with-netsnmp               Specify the location of net-snmp.])
AC_ARG_WITH(graphviz,
[  --with-graphviz              Specify the location of graphviz.])

dnl ------------
dnl Check LINKBASE
dnl ------------
AC_ARG_WITH(linkbase,
[  --with-linkbase              Set local library base for use in compilation.])

dnl -----------------------------------------
dnl If linkbase specified expand it
dnl -----------------------------------------
if test "x${with_linkbase}" != "x" ; then
  CFLAGS="$CFLAGS -I${with_linkbase}/include" ;
  LIBS="$LIBS -L${with_linkbase}/lib" ;
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB(m, main)

dnl ------------------
dnl check SNMP library
dnl ------------------

AC_MSG_CHECKING(for snmp library)
  saved_libs="${LIBS}"
  saved_ldflags="${LDFLAGS}"
  LIBS=""
  LDFLAGS=""

  link_dirs="${with_netsnmp} ${with_linkbase} /usr/local"
  for link_dir in ${link_dirs}; do
    test -z $link_dir && continue;
    LDFLAGS="-Wl,--rpath,${link_dir}/lib -L${link_dir}/lib"

    AC_CHECK_LIB(crypto, main, [CRYPTO=-lcrypto])
    if test "x${CRYPTO}" != "x"; then
      LIBS="$LIBS ${CRYPTO}"
    fi

    snmpnames="netsnmp snmp"
    for snmpname in ${snmpnames}; do
      test -z $snmpname && continue;

      AC_MSG_NOTICE(try linking with LIBS: $LIBS LDFLAGS: $LDFLAGS)
      AC_CHECK_LIB($snmpname, asn_parse_int,
        [HAVE_SNMP=yes; ac_snmp_lib="$LIBS -l${snmpname}";
         ac_snmp_ldflags="$LDFLAGS"],,${CRYPTO})
      test "${HAVE_SNMP}" = "yes" && break
    done
    test "${HAVE_SNMP}" = "yes" && break
  done

  if test "x${HAVE_SNMP}" != "x"; then
    SNMP_LIBS="$ac_snmp_lib"
    SNMP_LDFLAGS="$ac_snmp_ldflags"
    AC_SUBST(SNMP_LIBS)
    AC_SUBST(SNMP_LDFLAGS)
    LIBS="${saved_libs}"
    LDFLAGS="${saved_ldflags}"
  fi
AC_MSG_RESULT($ac_snmp_lib)
if test "x${with_netsnmp}" != "x" && "x${HAVE_SNMP}" = "x"; then
  AC_MSG_ERROR(cannot find valid snmp library)
fi

AC_MSG_CHECKING(for snmp library header)
  link_dirs="${with_netsnmp} ${with_linkbase} /usr/local"
  ac_snmp_dirs="net-snmp net-snmp/library ucd-snmp"

  for link_dir in ${link_dirs}; do
    test -z $link_dir && continue
    for ac_snmp_dir in ${ac_snmp_dirs}; do
      test -z $ac_snmp_dir && continue
      ac_snmps="${ac_snmps} ${link_dir}/include/${ac_snmp_dir}/asn1.h"
    done
  done

  for ac_snmp in ${ac_snmps}; do
    test -z ${ac_snmp} && continue
    AC_MSG_NOTICE(search for ${ac_snmp})
    test -f ${ac_snmp} && break
  done
  if test "x${ac_snmp}" != "x"; then
    AC_MSG_NOTICE(found: ${ac_snmp})
    ac_snmp_header=`echo ${ac_snmp} | sed -e 's/\/asn1.h//'`
    ac_snmp_header="-I${ac_snmp_header}"

    case ${ac_snmp} in
      */net-snmp/*)
        AC_DEFINE(HAVE_SNMP,,SNMP)
        AC_DEFINE(HAVE_NETSNMP,,NET-SNMP)
        ac_snmp_header="-I${link_dir}/include"
        ;;
      */ucd-snmp/*)
        AC_DEFINE(HAVE_SNMP,,SNMP)
        ;;
    esac

    SNMP_CFLAGS="${ac_snmp_header}"
    AC_SUBST(SNMP_CFLAGS)
  fi
AC_MSG_RESULT($ac_snmp_header)
if test "x${with_netsnmp}" != "x" && "x${ac_snmp}" = "x"; then
  AC_MSG_ERROR(cannot find valid snmp library header)
fi

# Check for graphviz
AC_MSG_CHECKING(for graphviz library)
  save_CFLAGS=$CFLAGS
  save_LDFLAGS=$LDFLAGS
  save_LIBS=$LIBS
  link_dirs="${with_graphviz} /usr/local"

  for link_dir in ${link_dirs}; do
    test -z $link_dir && continue
    CFLAGS="$CFLAGS -I${link_dir}/include/graphviz"
    LDFLAGS="$LDFLAGS -Wl,--rpath -Wl,${link_dir}/lib/graphviz -L${link_dir}/lib/graphviz"
    LIBS="$LIBS -lgvc"

AC_LINK_IFELSE(
  AC_LANG_SOURCE([[
#include <gvc.h>
int main () { aginit (); return 0; } ]]),
[ HAVE_LIBGRAPH=yes; AC_MSG_RESULT(yes) ],
[ AC_MSG_RESULT(no) ])

    if test "${HAVE_LIBGRAPH+set}" != set; then
      AC_MSG_WARN(graph library not found in ${link_dir}/include/graphviz and ${link_dir}/lib/graphviz)
      CFLAGS=$save_CFLAGS
      LDFLAGS=$save_LDFLAGS
      LIBS=$save_LIBS
    else
      AC_MSG_RESULT(graphviz is found in ${linkdir})
      AC_DEFINE(HAVE_GRAPHVIZ,1,graphviz)
      GRAPHVIZ_CFLAGS="-I${link_dir}/include/graphviz"
      GRAPHVIZ_LDFLAGS="-Wl,--rpath -Wl,${link_dir}/lib/graphviz -L${link_dir}/lib/graphviz"
      GRAPHVIZ_LIBS="-lgvc"
      AC_SUBST(GRAPHVIZ_CFLAGS)
      AC_SUBST(GRAPHVIZ_LDFLAGS)
      AC_SUBST(GRAPHVIZ_LIBS)
      CFLAGS=$save_CFLAGS
      LDFLAGS=$save_LDFLAGS
      LIBS=$save_LIBS
      break
    fi
  done
if test "x${with_graphviz}" != "x" && "x${HAVE_GRAPHVIZ}" = "x"; then
  AC_MSG_ERROR(cannot find valid graphviz library)
fi

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
#AC_FUNC_MALLOC
AC_FUNC_MEMCMP
#AC_FUNC_REALLOC
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday memmove memset strdup strerror strstr])
AC_CONFIG_FILES([Makefile lib/Makefile network/Makefile function/Makefile traffic-model/Makefile routing/Makefile interface/Makefile])
AC_OUTPUT

echo "
srsc configuration
-------------------
version                 : ${VERSION}
compiler                : ${CC}
compiler flags          : ${CFLAGS}
linker flags            : ${LDFLAGS}
link libraries          : ${LIBS}
"


