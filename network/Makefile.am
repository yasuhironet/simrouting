## Process this file with automake to produce Makefile.in.

INCLUDES = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib

noinst_LIBRARIES = libnetwork.a

libnetwork_a_SOURCES = \
	graph.c graph_cmd.c routing.c weight.c network.c \
	group.c path.c forward.c tag-hash.c

noinst_HEADERS = \
	graph.h graph_cmd.h routing.h weight.h network.h \
	group.h path.h

