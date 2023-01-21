## Process this file with automake to produce Makefile.in.

INCLUDES = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib

noinst_LIBRARIES = libinterface.a

AM_CFLAGS = ${CFLAGS} @SNMP_CFLAGS@ @GRAPHVIZ_CFLAGS@
AM_LDFLAGS = ${LDFLAGS} @SNMP_LDFLAGS@ @GRAPHVIZ_LDFLAGS@

libinterface_a_SOURCES = \
	snmp.c ospf.c brite.c graphviz.c rocketfuel.c simrouting_file.c \
	ampl.c spring_os.c

noinst_HEADERS = \
	snmp.h ospf.h brite.h graphviz.h rocketfuel.h simrouting_file.h \
	ampl.h spring_os.h

