# first entries are for gnu make, 2nd for BSD make.  see http://lists.uzbl.org/pipermail/uzbl-dev-uzbl.org/2009-July/000177.html

CFLAGS:=-std=c99 $(shell pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0) -ggdb -Wall -W -DARCH="\"$(shell uname -m)\"" -lgthread-2.0 -DCOMMIT="\"$(shell git log | head -n1 | sed "s/.* //")\"" $(CPPFLAGS) -fPIC -W -Wall -Wextra -pedantic -ggdb3
CFLAGS!=echo -std=c99 `pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0` -ggdb -Wall -W -DARCH='"\""'`uname -m`'"\""' -lgthread-2.0 -DCOMMIT='"\""'`git log | head -n1 | sed "s/.* //"`'"\""' $(CPPFLAGS) -fPIC -W -Wall -Wextra -pedantic -ggdb3

LDFLAGS:=$(shell pkg-config --libs gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0) -pthread $(LDFLAGS)
LDFLAGS!=echo `pkg-config --libs gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0` -pthread $(LDFLAGS)

SRC = uzbl-core.c events.c callbacks.c inspector.c
OBJ = ${SRC:.c=.o}

all: uzbl-browser options

options:
	@echo
	@echo BUILD OPTIONS:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo
	@echo See the README file for usage instructions.


.c.o:
	@echo COMPILING $<
	@${CC} -c ${CFLAGS} $<
	@echo ... done.

${OBJ}: uzbl-core.h events.h callbacks.h inspector.h config.h

uzbl-core: ${OBJ}
	@echo
	@echo LINKING object files
	@${CC} -o $@ ${OBJ} ${LDFLAGS}
	@echo ... done.
	@echo Stripping binary
	@strip $@
	@echo ... done.


uzbl-browser: uzbl-core

# packagers, set DESTDIR to your "package directory" and PREFIX to the prefix you want to have on the end-user system
PREFIX?=/usr/local
INSTALLDIR?=$(DESTDIR)$(PREFIX)

# the 'tests' target can never be up to date
.PHONY: tests
force:

# When compiling unit tests, compile uzbl as a library first
tests: ${OBJ} force
	$(CC) -shared -Wl ${OBJ} -o ./tests/libuzbl-core.so
	cd ./tests/; $(MAKE)

test: uzbl-core
	                    ./uzbl-core --uri http://www.uzbl.org --verbose

test-browser: uzbl-browser
	PATH="`pwd`:$$PATH" ./uzbl-browser --uri http://www.uzbl.org --verbose

test-dev: uzbl-core
	XDG_DATA_HOME=./examples/data                    XDG_CONFIG_HOME=./examples/config                                        ./uzbl-core --uri http://www.uzbl.org --verbose

test-dev-browser: uzbl-browser
	XDG_DATA_HOME=./examples/data   XDG_CACHE_HOME=./examples/cache   XDG_CONFIG_HOME=./examples/config   PATH="`pwd`:$$PATH" ./examples/data/uzbl/scripts/cookie_daemon.py start -nv &
	XDG_DATA_HOME=./examples/data   XDG_CACHE_HOME=./examples/cache   XDG_CONFIG_HOME=./examples/config   PATH="`pwd`:$$PATH" ./examples/data/uzbl/scripts/event_manager.py start -nv &
	XDG_DATA_HOME=./examples/data   XDG_CACHE_HOME=./examples/cache   XDG_CONFIG_HOME=./examples/config   PATH="`pwd`:`pwd`/examples/data/uzbl/scripts/:$$PATH" ./uzbl-browser --uri http://www.uzbl.org --verbose
	XDG_DATA_HOME=./examples/data   XDG_CACHE_HOME=./examples/cache   XDG_CONFIG_HOME=./examples/config   PATH="`pwd`:$$PATH" ./examples/data/uzbl/scripts/cookie_daemon.py stop -v
	XDG_DATA_HOME=./examples/data   XDG_CACHE_HOME=./examples/cache   XDG_CONFIG_HOME=./examples/config   PATH="`pwd`:$$PATH" ./examples/data/uzbl/scripts/event_manager.py stop -v

test-share: uzbl-core
	XDG_DATA_HOME=${INSTALLDIR}/share/uzbl/examples/data XDG_CONFIG_HOME=${INSTALLDIR}/share/uzbl/examples/config                     ./uzbl-core --uri http://www.uzbl.org --verbose

test-share-browser: uzbl-browser
	XDG_DATA_HOME=${INSTALLDIR}/share/uzbl/examples/data XDG_CONFIG_HOME=${INSTALLDIR}/share/uzbl/examples/config PATH="`pwd`:$$PATH" ./uzbl-browser --uri http://www.uzbl.org --verbose

clean:
	rm -f uzbl-core
	rm -f uzbl-core.o
	rm -f events.o
	rm -f callbacks.o
	rm -f inspector.o
	cd ./tests/; $(MAKE) clean

install: install-uzbl-core install-uzbl-browser install-uzbl-tabbed

install-uzbl-core: all
	install -d $(INSTALLDIR)/bin
	install -d $(INSTALLDIR)/share/uzbl/docs
	install -d $(INSTALLDIR)/share/uzbl/examples
	cp -rp docs     $(INSTALLDIR)/share/uzbl/
	cp -rp config.h $(INSTALLDIR)/share/uzbl/docs/
	cp -rp examples $(INSTALLDIR)/share/uzbl/
	install -m755 uzbl-core    $(INSTALLDIR)/bin/uzbl-core
	install -m644 AUTHORS      $(INSTALLDIR)/share/uzbl/docs
	install -m644 README       $(INSTALLDIR)/share/uzbl/docs

install-uzbl-browser: all
	install -d $(INSTALLDIR)/bin
	install -m755 uzbl-browser $(INSTALLDIR)/bin/uzbl-browser
	install -m755 examples/data/uzbl/scripts/cookie_daemon.py $(INSTALLDIR)/bin/cookie_daemon.py
	install -m755 examples/data/uzbl/scripts/event_manager.py $(INSTALLDIR)/bin/event_manager.py
	sed -i 's#^PREFIX=.*#PREFIX=$(PREFIX)#' $(INSTALLDIR)/bin/uzbl-browser
	sed -i "s#^PREFIX = None#PREFIX = '$(PREFIX)'#" $(INSTALLDIR)/bin/event_manager.py

install-uzbl-tabbed: all
	install -d $(INSTALLDIR)/bin
	install -m755 examples/data/uzbl/scripts/uzbl_tabbed.py $(INSTALLDIR)/bin/uzbl-tabbed

uninstall:
	rm -rf $(INSTALLDIR)/bin/uzbl-*
	rm -rf $(INSTALLDIR)/bin/cookie_daemon.py
	rm -rf $(INSTALLDIR)/bin/event_manager.py
	rm -rf $(INSTALLDIR)/share/uzbl
