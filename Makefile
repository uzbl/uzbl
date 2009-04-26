CPPFLAGS=$(shell pkg-config --cflags gtk+-2.0 webkit-1.0) -Wall -W
LDFLAGS=$(shell pkg-config --libs gtk+-2.0 webkit-1.0)
all: uzbl

test:
	./uzbl --uri http://www.archlinux.org

clean:
	rm -f uzbl
