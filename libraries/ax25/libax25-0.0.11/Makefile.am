
lib_LTLIBRARIES = libax25.la libax25io.la

libax25includedir = $(includedir)/netax25
libax25ioincludedir = $(includedir)/netax25

INCLUDES = -DAX25_SYSCONFDIR=\""$(sysconfdir)/ax25/"\" \
	   -DAX25_LOCALSTATEDIR=\""$(localstatedir)/ax25/"\" 

libax25include_HEADERS = 	\
	axlib.h		\
	axconfig.h	\
	nrconfig.h	\
	rsconfig.h	\
	daemon.h	\
	ttyutils.h	\
	procutils.h	\
	kernel_ax25.h	\
	kernel_rose.h	\
	kernel_netrom.h	\
	mheard.h	

libax25ioinclude_HEADERS = 	\
	ax25io.h

libax25_la_SOURCES = \
	axconfig.c \
	axconfig.h \
	axlib.h \
	axutils.c \
	config.h \
	daemon.c \
	daemon.h \
	mheard.h \
	nrconfig.c \
	nrconfig.h \
	pathnames.h \
	procutils.c \
	procutils.h \
	rsconfig.c \
	rsconfig.h \
	ttyutils.c \
	ttyutils.h  \
	kernel_ax25.h \
	kernel_rose.h
	
libax25io_la_SOURCES = \
	ax25io.c \
	ax25io.h 

man_MANS = ax25.3 rose.3 axports.5 rsports.5 nrports.5


EXTRA_DIST= $(man_MANS) glibc.patch $(etcfiles)

AX25_SYSCONFDIR=@sysconfdir@/ax25/
AX25_LOCALSTATEDIR=@localstatedir@/ax25/

# Needed so that the install is optional
etcdir = $(AX25_SYSCONFDIR)
etcfiles =  axports rsports nrports
installconf:
	$(mkinstalldirs) $(DESTDIR)$(etcdir)
	@list='$(etcfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(etcdir)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(etcdir)/$$p; \
	done

