CPPFLAGS=$(shell pkg-config --cflags gtk+-2.0 webkit-1.0) -Wall -W
LDFLAGS=$(shell pkg-config --libs gtk+-2.0 webkit-1.0)
all: uzbl

test:
	./uzbl --uri http://www.archlinux.org

clean:
	rm -f uzbl

install:
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/uzbl/docs
	install -d $(DESTDIR)/usr/share/uzbl/example-scripts
	install -D -m755 uzbl $(DESTDIR)/usr/bin/uzbl
	install -D -m644 extra/* $(DESTDIR)/usr/share/uzbl/example-scripts
	install -D -m644 README $(DESTDIR)/usr/share/uzbl/docs
