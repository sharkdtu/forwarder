include Make.vars

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include lib

bin_PROGRAMS = forwarder 

forwarder_SOURCES = include/forwarder.hh   \
                    core/forwarder.cc      \
                    forwarder_main.cc

forwarder_CPPFLAGS = $(AM_CPPFLAGS)

forwarder_LDFLAGS = $(AM_LDFLAGS)

forwarder_LDADD = $(LDADD)                       \
                  $(builddir)/lib/libforwarder.a 

