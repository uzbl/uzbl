# first entries are for gnu make, 2nd for BSD make.  see http://lists.uzbl.org/pipermail/uzbl-dev-uzbl.org/2009-July/000177.html

CFLAGS:=-std=c99 $(shell pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0) -ggdb -Wall -W -DARCH="\"$(shell uname -m)\"" -lgthread-2.0 -DCOMMIT="\"$(shell ./hash.sh)\"" $(CPPFLAGS) -fPIC -W -Wall -Wextra -pedantic
CFLAGS!=echo -std=c99 `pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0` -ggdb -Wall -W -DARCH='"\""'`uname -m`'"\""' -lgthread-2.0 -DCOMMIT='"\""'`./hash.sh`'"\""' $(CPPFLAGS) -fPIC -W -Wall -Wextra -pedantic

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
# end-users who build from source: don't care about DESTDIR, update PREFIX if you want to
# RUN_PREFIX : what the prefix is when the software is run. usually the same as PREFIX
PREFIX?=/usr/local
INSTALLDIR?=$(DESTDIR)$(PREFIX)
RUN_PREFIX?=$(PREFIX)

# the 'tests' target can never be up to date
.PHONY: tests
force:

# When compiling unit tests, compile uzbl as a library first
tests: ${OBJ} force
	$(CC) -shared -Wl ${OBJ} -o ./tests/libuzbl-core.so
	cd ./tests/; $(MAKE)

test-uzbl-core: uzbl-core
	./uzbl-core --uri http://www.uzbl.org --verbose

test-uzbl-browser: uzbl-browser
	./uzbl-browser --uri http://www.uzbl.org --verbose

test-uzbl-core-sandbox: uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-core
	source ./sandbox/env.sh && uzbl-core --uri http://www.uzbl.org --verbose
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-browser-sandbox: uzbl-browser
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-browser
	source ./sandbox/env.sh && uzbl-cookie-daemon restart -nv &
	source ./sandbox/env.sh && uzbl-event-manager restart -nav &
	source ./sandbox/env.sh && uzbl-browser --uri http://www.uzbl.org --verbose
	source ./sandbox/env.sh && uzbl-cookie-daemon stop -v
	source ./sandbox/env.sh && uzbl-event-manager stop -v
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

clean:
	rm -f uzbl-core
	rm -f uzbl-core.o
	rm -f events.o
	rm -f callbacks.o
	rm -f inspector.o
	find examples/ -name "*.pyc" -delete
	cd ./tests/; $(MAKE) clean
	rm -rf ./sandbox/{examples,usr}/

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
	sed -i 's#^set prefix.*=.*#set prefix     = $(RUN_PREFIX)#' $(INSTALLDIR)/share/uzbl/examples/config/uzbl/config

install-uzbl-browser:
	install -d $(INSTALLDIR)/bin
	install -m755 uzbl-browser $(INSTALLDIR)/bin/uzbl-browser
	install -m755 examples/data/uzbl/scripts/uzbl-cookie-daemon $(INSTALLDIR)/bin/uzbl-cookie-daemon
	install -m755 examples/data/uzbl/scripts/uzbl-event-manager $(INSTALLDIR)/bin/uzbl-event-manager
	sed -i 's#^PREFIX=.*#PREFIX=$(RUN_PREFIX)#' $(INSTALLDIR)/bin/uzbl-browser
	sed -i "s#^PREFIX = .*#PREFIX = '$(RUN_PREFIX)'#" $(INSTALLDIR)/bin/uzbl-event-manager

install-uzbl-tabbed:
	install -d $(INSTALLDIR)/bin
	install -m755 examples/data/uzbl/scripts/uzbl-tabbed $(INSTALLDIR)/bin/uzbl-tabbed

uninstall:
	rm -rf $(INSTALLDIR)/bin/uzbl-*
	rm -rf $(INSTALLDIR)/share/uzbl
