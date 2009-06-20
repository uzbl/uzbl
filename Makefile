CFLAGS:=-std=c99 $(shell pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4) -ggdb -Wall -W -DARCH="\"$(shell uname -m)\"" -lgthread-2.0 -DG_ERRORCHECK_MUTEXES -DCOMMIT="\"$(shell git log | head -n1 | sed "s/.* //")\"" $(CPPFLAGS)
LDFLAGS:=$(shell pkg-config --libs gtk+-2.0 webkit-1.0 libsoup-2.4) -pthread $(LDFLAGS)
all: uzbl uzblctrl

PREFIX?=$(DESTDIR)/usr

test: uzbl
	./uzbl --uri http://www.uzbl.org --verbose

test-dev: uzbl
	XDG_DATA_HOME=./examples/data               XDG_CONFIG_HOME=./examples/config               ./uzbl --uri http://www.uzbl.org --verbose

test-share: uzbl
	XDG_DATA_HOME=/usr/share/uzbl/examples/data XDG_CONFIG_HOME=/usr/share/uzbl/examples/config ./uzbl --uri http://www.uzbl.org --verbose

	
clean:
	rm -f uzbl
	rm -f uzblctrl

install:
	install -d $(PREFIX)/bin
	install -d $(PREFIX)/share/uzbl/docs
	install -d $(PREFIX)/share/uzbl/examples
	install -m755 uzbl $(PREFIX)/bin/uzbl
	install -m755 uzblctrl $(PREFIX)/bin/uzblctrl
	cp -ax docs     $(PREFIX)/share/uzbl/
	cp -ax config.h $(PREFIX)/share/uzbl/docs/
	cp -ax examples $(PREFIX)/share/uzbl/
	install -m644 AUTHORS $(PREFIX)/share/uzbl/docs
	install -m644 README  $(PREFIX)/share/uzbl/docs


uninstall:
	rm -rf $(PREFIX)/bin/uzbl
	rm -rf $(PREFIX)/bin/uzblctrl
	rm -rf $(PREFIX)/share/uzbl
