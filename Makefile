LIBS    := gtk+-2.0 webkit-1.0
ARCH    := $(shell uname -m)
COMMIT  := $(shell git log | head -n1 | sed "s/.* //")
DEBUG   := -ggdb -Wall -W -DG_ERRORCHECK_MUTEXES 

CFLAGS  := $(shell --cflags $(LIBS)) $(DEBUG) -DARCH="$(ARCH)" -DCOMMIT="\"$(COMMIT)\""
LDFLAGS := $(shell --libs $(LIBS))

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
UZBLDATA?= $(DATADIR)/uzbl
DOCSDIR ?= $(UZBLDATA)/docs
EXMPLSDIR ?= $(UZBLDATA)/examples

all: uzbl uzblctrl

uzbl: uzbl.c uzbl.h config.h

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LIBS) -o $@ $<

test: uzbl
	./uzbl --uri http://www.uzbl.org

test-config: uzbl
	./uzbl --uri http://www.uzbl.org < examples/configs/sampleconfig-dev

test-config-real: uzbl
	./uzbl --uri http://www.uzbl.org < $(UZBLDATA)/examples/configs/sampleconfig
	
clean:
	rm -f uzbl
	rm -f uzblctrl

install:
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(DOCSDIR)
	install -d $(DESTDIR)$(EXMPLSDIR)
	install -D -m755 uzbl $(DESTDIR)$(BINDIR)/uzbl
	install -D -m755 uzblctrl $(DESTDIR)$(BINDIR)/uzblctrl
	cp -ax docs/*     $(DESTDIR)$(DOCSDIR)
	cp -ax config.h   $(DESTDIR)$(DOCSDIR)
	cp -ax examples/* $(DESTDIR)$(EXMPLSDIR)
	install -D -m644 AUTHORS $(DESTDIR)$(DOCSDIR)
	install -D -m644 README  $(DESTDIR)$(DOCSDIR)

uninstall:
	rm -rf $(DESTDIR)$(BINDIR)/uzbl
	rm -rf $(DESTDIR)$(BINDIR)/uzblctrl
	rm -rf $(DESTDIR)$(UZBLDATA)
