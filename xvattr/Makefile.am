# 

bin_PROGRAMS = xvattr 
xvattr_SOURCES = xvattr.c getopt1.c

xvattr_LDADD = $(X_LIBS) $(X_PRE_LIBS) $(X_EXTRA_LIBS) -lX11 -lXext

noinst_HEADERS = getopt.h

# Documentation
EXT = 1
man1_MANS = $(PACKAGE).$(EXT)

EXTRA_DIST = $(PACKAGE).pod $(man1_MANS) $(PACKAGE).html


$(PACKAGE).html: $(PACKAGE).pod
	pod2html --infile=$< --outfile=$@

$(PACKAGE).$(EXT): $(PACKAGE).pod
	pod2man --center='User Commands' $(PACKAGE).pod >$@
