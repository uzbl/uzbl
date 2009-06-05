CPPFLAGS:=$(shell pkg-config --cflags gtk+-2.0 webkit-1.0) -ggdb -Wall -W -std=gnu99 -DARCH="\"$(shell uname -m)\"" -DG_ERRORCHECK_MUTEXES -DCOMMIT="\"$(shell git log | head -n1 | sed "s/.* //")\"" $(CPPFLAGS)
LDFLAGS:=$(shell pkg-config --libs gtk+-2.0 webkit-1.0) $(LDFLAGS)
all: uzbl uzblctrl

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
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/uzbl/docs
	install -d $(DESTDIR)/usr/share/uzbl/examples
	install -D -m755 uzbl $(DESTDIR)/usr/bin/uzbl
	install -D -m755 uzblctrl $(DESTDIR)/usr/bin/uzblctrl
	cp -ax docs     $(DESTDIR)/usr/share/uzbl/
	cp -ax config.h $(DESTDIR)/usr/share/uzbl/docs/
	cp -ax examples $(DESTDIR)/usr/share/uzbl/
	cp -ax uzbl.png $(DESTDIR)/usr/share/uzbl/
	install -D -m644 AUTHORS $(DESTDIR)/usr/share/uzbl/docs
	install -D -m644 README  $(DESTDIR)/usr/share/uzbl/docs


uninstall:
	rm -rf $(DESTDIR)/usr/bin/uzbl
	rm -rf $(DESTDIR)/usr/bin/uzblctrl
	rm -rf $(DESTDIR)/usr/share/uzbl
