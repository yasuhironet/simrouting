## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/lib/libcore

noinst_PROGRAMS = simrouting star-route

star_route_SOURCES = \
	star-route.c

simrouting_SOURCES = \
	simrouting.c

noinst_HEADERS = \
	star-route.h

star_route_LDFLAGS = \
	@SNMP_LDFLAGS@ @GRAPHVIZ_LDFLAGS@

simrouting_LDFLAGS = \
	@SNMP_LDFLAGS@ @GRAPHVIZ_LDFLAGS@

star_route_LDADD = \
	../lib/libcore/module.o \
	../lib/network/libnetwork.a \
	../lib/interface/libinterface.a \
	../lib/routing/librouting.a \
	../lib/function/libfunction.a \
	../lib/traffic-model/libtraffic.a \
	../lib/libcore/libcore.a \
	@SNMP_LIBS@ @GRAPHVIZ_LIBS@

simrouting_LDADD = \
	../lib/libcore/module.o \
	../lib/network/libnetwork.a \
	../lib/interface/libinterface.a \
	../lib/routing/librouting.a \
	../lib/function/libfunction.a \
	../lib/traffic-model/libtraffic.a \
	../lib/libcore/libcore.a \
	@SNMP_LIBS@ @GRAPHVIZ_LIBS@

