# 

bin_PROGRAMS = xvattr gxvattr
xvattr_SOURCES = xvattr.c

xvattr_LDADD = $(X_LIBS) $(X_PRE_LIBS) $(X_EXTRA_LIBS) -lX11 -lXext @LIBOBJS@

gxvattr_SOURCES = gxvattr.c
CFLAGS = @GTK_CFLAGS@
gxvattr_LDADD = @GTK_LIBS@ $(X_LIBS) $(X_PRE_LIBS) $(X_EXTRA_LIBS) -lX11 -lXext
noinst_HEADERS = getopt.h

# Documentation
EXT = 1
man1_MANS = $(PACKAGE).$(EXT)

EXTRA_DIST = $(PACKAGE).pod $(man1_MANS) $(PACKAGE).html getopt.h

$(PACKAGE).html: $(PACKAGE).pod
	pod2html --infile=$< --outfile=$@

$(PACKAGE).$(EXT): $(PACKAGE).pod
	pod2man --center='User Commands' $(PACKAGE).pod >$@
