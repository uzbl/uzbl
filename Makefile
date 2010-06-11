# first entries are for gnu make, 2nd for BSD make.  see http://lists.uzbl.org/pipermail/uzbl-dev-uzbl.org/2009-July/000177.html

CFLAGS:=-std=c99 $(shell pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0) -ggdb -Wall -W -DARCH="\"$(shell uname -m)\"" -lgthread-2.0 -DCOMMIT="\"$(shell ./misc/hash.sh)\"" $(CPPFLAGS) -fPIC -W -Wall -Wextra -pedantic
CFLAGS!=echo -std=c99 `pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0` -ggdb -Wall -W -DARCH='"\""'`uname -m`'"\""' -lgthread-2.0 -DCOMMIT='"\""'`./misc/hash.sh`'"\""' $(CPPFLAGS) -fPIC -W -Wall -Wextra -pedantic

LDFLAGS:=$(shell pkg-config --libs gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0) -pthread $(LDFLAGS)
LDFLAGS!=echo `pkg-config --libs gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0` -pthread $(LDFLAGS)

SRC = $(wildcard src/*.c)
HEAD = $(wildcard src/*.h)
OBJ = $(foreach obj, $(SRC:.c=.o), $(notdir $(obj)))

all: uzbl-browser

VPATH:=src

.c.o:
	@echo -e "${CC} -c ${CFLAGS} $<"
	@${CC} -c ${CFLAGS} $<

${OBJ}: ${HEAD}

uzbl-core: ${OBJ}
	@echo -e "\n${CC} -o $@ ${OBJ} ${LDFLAGS}"
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

uzbl-browser: uzbl-core

# packagers, set DESTDIR to your "package directory" and PREFIX to the prefix you want to have on the end-user system
# end-users who build from source: don't care about DESTDIR, update PREFIX if you want to
# RUN_PREFIX : what the prefix is when the software is run. usually the same as PREFIX
PREFIX?=/usr/local
INSTALLDIR?=$(DESTDIR)$(PREFIX)
DOCDIR?=$(INSTALLDIR)/share/uzbl/docs
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
	./src/uzbl-browser --uri http://www.uzbl.org --verbose

test-uzbl-core-sandbox: uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-example-data
	cp -np ./misc/env.sh ./sandbox/env.sh
	source ./sandbox/env.sh && uzbl-core --uri http://www.uzbl.org --verbose
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-browser-sandbox: uzbl-browser
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-browser
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-example-data
	cp -np ./misc/env.sh ./sandbox/env.sh
	source ./sandbox/env.sh && uzbl-cookie-daemon restart -nv &
	source ./sandbox/env.sh && uzbl-event-manager restart -navv &
	source ./sandbox/env.sh && uzbl-browser --uri http://www.uzbl.org --verbose
	source ./sandbox/env.sh && uzbl-cookie-daemon stop -v
	source ./sandbox/env.sh && uzbl-event-manager stop -ivv
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

clean:
	rm -f uzbl-core
	rm -f uzbl-core.o
	rm -f events.o
	rm -f callbacks.o
	rm -f inspector.o
	find ./examples/ -name "*.pyc" -delete
	cd ./tests/; $(MAKE) clean
	rm -rf ./sandbox/

strip:
	@echo Stripping binary
	@strip uzbl-core
	@echo ... done.

install: install-uzbl-core install-uzbl-browser install-uzbl-tabbed

install-dirs:
	[ -d "$(INSTALLDIR)/bin" ] && install -d -m755 $(INSTALLDIR)/bin
install-uzbl-core: all install-dirs
	install -d $(INSTALLDIR)/share/uzbl/
	install -d $(DOCDIR)
	install -m644 docs/* $(DOCDIR)/
	install -m644 src/config.h $(DOCDIR)/
	install -m644 README $(DOCDIR)/
	install -m644 AUTHORS $(DOCDIR)/
	cp -r examples $(INSTALLDIR)/share/uzbl/
	chmod 755 $(INSTALLDIR)/share/uzbl/examples/data/scripts/*
	mv $(INSTALLDIR)/share/uzbl/examples/config/config{,.bak}
	sed 's#^set prefix.*=.*#set prefix     = $(RUN_PREFIX)#' < $(INSTALLDIR)/share/uzbl/examples/config/config > $(INSTALLDIR)/share/uzbl/examples/config/config
	rm $(INSTALLDIR)/share/uzbl/examples/config/config.bak
	install -m755 uzbl-core $(INSTALLDIR)/bin/uzbl-core

install-uzbl-browser: install-dirs
	install -m755 src/uzbl-browser $(INSTALLDIR)/bin/uzbl-browser
	install -m755 examples/data/scripts/uzbl-cookie-daemon $(INSTALLDIR)/bin/uzbl-cookie-daemon
	install -m755 examples/data/scripts/uzbl-event-manager $(INSTALLDIR)/bin/uzbl-event-manager
	mv $(INSTALLDIR)/bin/uzbl-browser{,.bak}
	sed 's#^PREFIX=.*#PREFIX=$(RUN_PREFIX)#' < $(INSTALLDIR)/bin/uzbl-browser > $(INSTALLDIR)/bin/uzbl-browser.bak
	rm $(INSTALLDIR)/bin/uzbl-browser.bak
	mv $(INSTALLDIR)/bin/uzbl-event-manager{,.bak}
	sed "s#^PREFIX = .*#PREFIX = '$(RUN_PREFIX)'#" < $(INSTALLDIR)/bin/uzbl-event-manager > $(INSTALLDIR)/bin/uzbl-event-manager.bak
	rm $(INSTALLDIR)/bin/uzbl-event-manager.bak

install-uzbl-tabbed: install-dirs
	install -m755 examples/data/scripts/uzbl-tabbed $(INSTALLDIR)/bin/uzbl-tabbed

# you probably only want to do this manually when testing and/or to the sandbox. not meant for distributors
install-example-data: install-dirs
	install -d $(DESTDIR)/home/.config/uzbl
	install -d $(DESTDIR)/home/.cache/uzbl
	install -d $(DESTDIR)/home/.local/share/uzbl
	cp -rp examples/config/* $(DESTDIR)/home/.config/uzbl/
	cp -rp examples/data/*   $(DESTDIR)/home/.local/share/uzbl/

uninstall:
	rm -rf $(INSTALLDIR)/bin/uzbl-*
	rm -rf $(INSTALLDIR)/share/uzbl
