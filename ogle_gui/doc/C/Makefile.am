
glade_helpdir = $(datadir)/gnome/help/ogle/C

glade_help_DATA =         \
        topic.dat


# Files that aren't in a binary/data/library target have to be listed here
# to be included in the tarball when you 'make dist'.
# We include the timestamp files so it won't even try to rebuild the docs
# unless you edit the SGML files.
EXTRA_DIST = \
	user-guide.sgml		\
	user-guide.stamp	\
        topic.dat


## The '-' before the command means to ignore it if it fails.  That way
## people can still build the software without the docbook tools.

all: user-guide.stamp 

user-guide.stamp: user-guide.sgml
	-db2html user-guide.sgml
	@echo timestamp > user-guide.stamp 2> /dev/null


# I haven't tested these yet.
user-guide.ps: user-guide.sgml
	-db2ps $<

user-guide.rtf: user-guide.sgml
	-db2rtf $<


## When we make dist, we include the generated HTML so people don't
## have to have the docbook tools.
dist-hook:
	mkdir $(distdir)/user-guide
#	mkdir $(distdir)/user-guide-images
#	mkdir $(distdir)/user-guide/stylesheet-images
	-cp user-guide/*.html user-guide/*.css $(distdir)/user-guide
	-cp user-guide-images/*.png $(distdir)/user-guide-images
	-cp user-guide/stylesheet-images/*.gif $(distdir)/user-guide/stylesheet-images
	-cp user-guide/stylesheet-images/*.png $(distdir)/user-guide/stylesheet-images
	-cp faq/*.html faq/*.css $(distdir)/faq
	-cp faq/stylesheet-images/*.gif $(distdir)/faq/stylesheet-images
	-cp faq/stylesheet-images/*.png $(distdir)/faq/stylesheet-images
	-cp turbo-start/*.html turbo-start/*.css $(distdir)/turbo-start
	-cp turbo-start/stylesheet-images/*.gif $(distdir)/turbo-start/stylesheet-images
	-cp turbo-start/stylesheet-images/*.png $(distdir)/turbo-start/stylesheet-images

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(glade_helpdir)/user-guide/user-guide-images
	$(mkinstalldirs) $(DESTDIR)$(glade_helpdir)/user-guide/stylesheet-images
	-for file in $(srcdir)/user-guide/*.html $(srcdir)/user-guide/*.css; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(glade_helpdir)/user-guide/$$basefile; \
	done
	-for file in $(srcdir)/user-guide-images/*.png; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(glade_helpdir)/user-guide/user-guide-images/$$basefile; \
	done
	-for file in $(srcdir)/user-guide/stylesheet-images/*.png $(srcdir)/user-guide/stylesheet-images/*.gif; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(glade_helpdir)/user-guide/stylesheet-images/$$basefile; \
	done
	$(mkinstalldirs) $(DESTDIR)$(glade_helpdir)/faq/stylesheet-images
	-for file in $(srcdir)/faq/*.html $(srcdir)/faq/*.css; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(glade_helpdir)/faq/$$basefile; \
	done
	-for file in $(srcdir)/faq/stylesheet-images/*.png $(srcdir)/faq/stylesheet-images/*.gif; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(glade_helpdir)/faq/stylesheet-images/$$basefile; \
	done
	$(mkinstalldirs) $(DESTDIR)$(glade_helpdir)/turbo-start/stylesheet-images
	-for file in $(srcdir)/turbo-start/*.html $(srcdir)/turbo-start/*.css; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(glade_helpdir)/turbo-start/$$basefile; \
	done
	-for file in $(srcdir)/turbo-start/stylesheet-images/*.png $(srcdir)/turbo-start/stylesheet-images/*.gif; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(glade_helpdir)/turbo-start/stylesheet-images/$$basefile; \
	done

clean:
	-rm -rf user-guide faq turbo-start
	-rm -f user-guide.stamp faq.stamp turbo-start.stamp
	-rm -rf user-guide.junk faq.junk turbo-start.junk
