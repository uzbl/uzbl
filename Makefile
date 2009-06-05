CPPFLAGS:=$(shell pkg-config --cflags gtk+-2.0 webkit-1.0) -ggdb -Wall -W -std=gnu99 -DARCH="\"$(shell uname -m)\"" -DG_ERRORCHECK_MUTEXES -DCOMMIT="\"$(shell git log | head -n1 | sed "s/.* //")\"" $(CPPFLAGS)
LDFLAGS:=$(shell pkg-config --libs gtk+-2.0 webkit-1.0) $(LDFLAGS)
all: uzbl uzblctrl

PREFIX?=$(DESTDIR)/usr

test: uzbl
	./uzbl --uri http://www.uzbl.org

test-config: uzbl
	./uzbl --uri http://www.uzbl.org < examples/configs/sampleconfig-dev

test-config-real: uzbl
	./uzbl --uri http://www.uzbl.org < /usr/share/uzbl/examples/configs/sampleconfig

clean:
	rm -f uzbl
	rm -f uzblctrl

install:
	install -d $(PREFIX)/bin
	install -d $(PREFIX)/share/uzbl/docs
	install -d $(PREFIX)/share/uzbl/examples
	install -D -m755 uzbl $(PREFIX)/bin/uzbl
	install -D -m755 uzblctrl $(PREFIX)/bin/uzblctrl
	cp -ax docs     $(PREFIX)/share/uzbl/
	cp -ax config.h $(PREFIX)/share/uzbl/docs/
	cp -ax examples $(PREFIX)/share/uzbl/
	cp -ax uzbl.png $(PREFIX)/share/uzbl/
	install -D -m644 AUTHORS $(PREFIX)/share/uzbl/docs
	install -D -m644 README  $(PREFIX)/share/uzbl/docs


uninstall:
	rm -rf $(PREFIX)/bin/uzbl
	rm -rf $(PREFIX)/bin/uzblctrl
	rm -rf $(PREFIX)/share/uzbl
