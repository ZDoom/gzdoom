# $Id: Makefile.am,v 1.15 2004/05/26 23:59:15 nuffer Exp $

bin_PROGRAMS = re2c
re2c_SOURCES = code.cc dfa.cc main.cc parser.cc actions.cc scanner.re substr.cc\
	translate.cc scanner.cc mbo_getopt.cc \
	basics.h dfa.h globals.h ins.h parser.h re.h scanner.h \
	substr.h token.h mbo_getopt.h
BUILT_SOURCES = parser.cc scanner.cc

man_MANS     = re2c.1

#CXXFLAGS     = -O2 -Wall -I. -Wno-unused -Wno-parentheses -Wno-deprecated
YFLAGS       = -d

RE2C         = re2c
RE2CFLAGS    = -s

CLEANFILES   = parser.cc y.tab.c y.tab.h parser.cc re2c.1 .version

DISTCLEANFILES = makerpm re2c.spec README scanner.cc re2c$(EXEEXT)

EXTRA_DIST   = $(man_MANS) README parser.y scanner.re makerpm.in re2c.spec.in \
               README.in y.tab.h \
               examples/basemmap.c examples/c.re examples/cmmap.re \
			   examples/cnokw.re examples/cunroll.re examples/modula.re \
			   examples/sample.re examples/simple.re examples/rexx/README \
			   examples/rexx/rexx.l examples/rexx/scanio.c doc/loplas.ps \
			   doc/sample.bib

rpm-files:	$(bin_PROGRAMS) $(EXTRA_DIST)

parser.cc:	$(top_srcdir)/parser.y
	$(YACC) $(YFLAGS) $<
	mv -f y.tab.c $(top_srcdir)/parser.cc
	if cmp -s y.tab.h $(top_srcdir)/y.tab.h; then :; else mv -f y.tab.h $(top_srcdir)/y.tab.h; fi

scanner.cc: $(top_srcdir)/scanner.re
	@if test -x ./re2c$(EXEEXT); then \
		echo "re2c $(RE2CFLAGS) -o $@ $<"; \
		./re2c $(RE2CFLAGS) -o $@ $< && cp $@ $(top_srcdir)/bootstrap/; \
	else \
		echo "cp -f $(top_srcdir)/bootstrap/$@ $@"; \
		cp -f $(top_srcdir)/bootstrap/$@ $@; \
	fi

.version:
	echo $(PACKAGE_VERSION) > .version
