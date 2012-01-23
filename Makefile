# packagers, set DESTDIR to your "package directory" and PREFIX to the prefix you want to have on the end-user system
# end-users who build from source: don't care about DESTDIR, update PREFIX if you want to
# RUN_PREFIX : what the prefix is when the software is run. usually the same as PREFIX
PREFIX?=/usr/local
INSTALLDIR?=$(DESTDIR)$(PREFIX)
DOCDIR?=$(INSTALLDIR)/share/uzbl/docs
RUN_PREFIX?=$(PREFIX)

# use GTK3-based webkit when it is available
USE_GTK3 = $(shell pkg-config --exists gtk+-3.0 webkitgtk-3.0 && echo 1)

ifeq ($(USE_GTK3),1)
	REQ_PKGS += gtk+-3.0 webkitgtk-3.0 javascriptcoregtk-3.0
	CPPFLAGS = -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
else
	REQ_PKGS += gtk+-2.0 webkit-1.0 javascriptcoregtk-1.0
	CPPFLAGS =
endif

# --- configuration ends here ---

REQ_PKGS += libsoup-2.4 gthread-2.0 glib-2.0

ARCH:=$(shell uname -m)

COMMIT_HASH:=$(shell ./misc/hash.sh)

CPPFLAGS += -DARCH=\"$(ARCH)\" -DCOMMIT=\"$(COMMIT_HASH)\"

PKG_CFLAGS:=$(shell pkg-config --cflags $(REQ_PKGS))

LDLIBS:=$(shell pkg-config --libs $(REQ_PKGS) x11)

CFLAGS += -std=c99 $(PKG_CFLAGS) -ggdb -W -Wall -Wextra -pedantic -pthread

SRC = $(wildcard src/*.c)
HEAD = $(wildcard src/*.h)
OBJ  = $(foreach obj, $(SRC:.c=.o),  $(notdir $(obj)))
LOBJ = $(foreach obj, $(SRC:.c=.lo), $(notdir $(obj)))

all: uzbl-browser

VPATH:=src

${OBJ}: ${HEAD}

uzbl-core: ${OBJ}

uzbl-browser: uzbl-core

# the 'tests' target can never be up to date
.PHONY: tests
force:

# this is here because the .so needs to be compiled with -fPIC on x86_64
${LOBJ}: ${SRC} ${HEAD}
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c src/$(@:.lo=.c) -o $@

# When compiling unit tests, compile uzbl as a library first
tests: ${LOBJ} force
	$(CC) -shared -Wl ${LOBJ} -o ./tests/libuzbl-core.so
	cd ./tests/; $(MAKE)

test-uzbl-core: uzbl-core
	./uzbl-core --uri http://www.uzbl.org --verbose

test-uzbl-browser: uzbl-browser
	./bin/uzbl-browser --uri http://www.uzbl.org --verbose

test-uzbl-core-sandbox: uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-example-data
	cp -np ./misc/env.sh ./sandbox/env.sh
	. ./sandbox/env.sh && uzbl-core --uri http://www.uzbl.org --verbose
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-browser-sandbox: uzbl-browser
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-browser
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-example-data
	cp -np ./misc/env.sh ./sandbox/env.sh
	-. ./sandbox/env.sh && uzbl-event-manager restart -avv
	. ./sandbox/env.sh && uzbl-browser --uri http://www.uzbl.org --verbose
	. ./sandbox/env.sh && uzbl-event-manager stop -ivv
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-tabbed-sandbox: uzbl-browser
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-browser
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-uzbl-tabbed
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-example-data
	cp -np ./misc/env.sh ./sandbox/env.sh
	-. ./sandbox/env.sh && uzbl-event-manager restart -avv
	. ./sandbox/env.sh && uzbl-tabbed
	. ./sandbox/env.sh && uzbl-event-manager stop -ivv
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

clean:
	rm -f uzbl-core
	rm -f *.o
	find ./examples/ -name "*.pyc" -delete
	cd ./tests/; $(MAKE) clean
	rm -rf ./sandbox/

strip:
	@echo Stripping binary
	@strip uzbl-core
	@echo ... done.

install: install-uzbl-core install-uzbl-browser install-uzbl-tabbed

install-dirs:
	[ -d "$(INSTALLDIR)/bin" ] || install -d -m755 $(INSTALLDIR)/bin

install-uzbl-core: all install-dirs
	install -d $(INSTALLDIR)/share/uzbl/
	install -d $(DOCDIR)
	install -m644 docs/* $(DOCDIR)/
	install -m644 src/config.h $(DOCDIR)/
	install -m644 README $(DOCDIR)/
	install -m644 AUTHORS $(DOCDIR)/
	install -m755 uzbl-core $(INSTALLDIR)/bin/uzbl-core

install-event-manager: install-dirs
	sed "s#^PREFIX = .*#PREFIX = '$(RUN_PREFIX)'#" < bin/uzbl-event-manager > $(INSTALLDIR)/bin/uzbl-event-manager
	chmod 755 $(INSTALLDIR)/bin/uzbl-event-manager

install-uzbl-browser: install-dirs install-uzbl-core install-event-manager
	sed 's#^PREFIX=.*#PREFIX=$(RUN_PREFIX)#' < bin/uzbl-browser > $(INSTALLDIR)/bin/uzbl-browser
	chmod 755 $(INSTALLDIR)/bin/uzbl-browser
	cp -r examples $(INSTALLDIR)/share/uzbl/
	chmod 755 $(INSTALLDIR)/share/uzbl/examples/data/scripts/*

install-uzbl-tabbed: install-dirs
	install -m755 bin/uzbl-tabbed $(INSTALLDIR)/bin/uzbl-tabbed

# you probably only want to do this manually when testing and/or to the sandbox. not meant for distributors
install-example-data:
	install -d $(DESTDIR)/home/.config/uzbl
	install -d $(DESTDIR)/home/.cache/uzbl
	install -d $(DESTDIR)/home/.local/share/uzbl
	cp -rp examples/config/* $(DESTDIR)/home/.config/uzbl/
	cp -rp examples/data/*   $(DESTDIR)/home/.local/share/uzbl/

uninstall:
	rm -rf $(INSTALLDIR)/bin/uzbl-*
	rm -rf $(INSTALLDIR)/share/uzbl
