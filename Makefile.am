## Process this file with automake to produce Makefile.in

SUBDIRS = lib network function routing traffic-model interface 

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/lib

noinst_PROGRAMS = simrouting

simrouting_SOURCES = \
	simrouting.c

noinst_HEADERS = \
	includes.h

simrouting_LDFLAGS = \
	@SNMP_LDFLAGS@ @GRAPHVIZ_LDFLAGS@

simrouting_LDADD = \
	lib/module.o \
	network/libnetwork.a \
	interface/libinterface.a \
	routing/librouting.a \
	function/libfunction.a \
	traffic-model/libtraffic.a \
	lib/libcore.a \
	@SNMP_LIBS@ @GRAPHVIZ_LIBS@

