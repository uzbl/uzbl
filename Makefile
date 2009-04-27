CPPFLAGS=$(shell pkg-config --cflags gtk+-2.0 webkit-1.0) -Wall -W
LDFLAGS=$(shell pkg-config --libs gtk+-2.0 webkit-1.0)
all: uzbl

test:
	./uzbl --uri http://www.uzbl.org

test-config:
	./uzbl --uri http://www.uzbl.org --config examples/configs/sampleconfig-dev

test-config-real:
	./uzbl --uri http://www.uzbl.org --config /usr/share/uzbl/examples/configs/sampleconfig
	
clean:
	rm -f uzbl

install:
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/uzbl/docs
	install -d $(DESTDIR)/usr/share/uzbl/examples
	install -D -m755 uzbl $(DESTDIR)/usr/bin/uzbl
	cp -ax examples $(DESTDIR)/usr/share/uzbl/
	install -D -m644 README $(DESTDIR)/usr/share/uzbl/docs
