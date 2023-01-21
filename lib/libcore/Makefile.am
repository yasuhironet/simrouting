## Process this file with automake to produce Makefile.in.

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/lib/libcore

noinst_LIBRARIES = libcore.a

libcore_a_SOURCES = \
	log.c termio.c vector.c shell.c command.c pqueue.c \
	command_shell.c table.c prefix.c file.c timer.c \
	module.c

noinst_HEADERS = \
	log.h termio.h vector.h shell.h command.h pqueue.h \
	command_shell.h table.h prefix.h file.h timer.h \
	module.h

