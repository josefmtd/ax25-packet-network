
etcfiles = ax25rtd.conf

installconf:
	$(mkinstalldirs) $(DESTDIR)$(AX25_SYSCONFDIR)
	@list='$(etcfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	done

sbin_PROGRAMS = ax25rtd ax25rtctl

LDADD = $(AX25_LIB)

EXTRA_DIST = $(etcfiles) $(doc_DATA)

docdir=${datadir}/doc/ax25-apps
doc_DATA= TODO.ax25rtd README.ax25rtd

ax25rtd_SOURCES =	\
	ax25rtd.c	\
	ax25rtd.h	\
	cache_ctl.c	\
	cache_dump.c	\
	config.c	\
	listener.c

AX25_SYSCONFDIR=$(sysconfdir)/ax25
AX25_LOCALSTATEDIR=$(localstatedir)/ax25

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	   -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 
