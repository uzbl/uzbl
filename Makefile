# packagers, set DESTDIR to your "package directory" and PREFIX to the prefix you want to have on the end-user system
# end-users who build from source: don't care about DESTDIR, update PREFIX if you want to
# RUN_PREFIX : what the prefix is when the software is run. usually the same as PREFIX
PREFIX?=/usr/local
DOCDIR?=$(PREFIX)/share/uzbl/docs
RUN_PREFIX?=$(PREFIX)

# use GTK3-based webkit when it is available
USE_GTK3 = $(shell pkg-config --exists gtk+-3.0 webkitgtk-3.0 && echo 1)

ifeq ($(USE_GTK3),1)
	REQ_PKGS += gtk+-3.0 webkitgtk-3.0 javascriptcoregtk-3.0
	CPPFLAGS = -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
else
	REQ_PKGS += gtk+-2.0 webkit-1.0
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
	make HOME=/home/sandbox DESTDIR=`pwd`/sandbox install-uzbl-core
	make DESTDIR=./sandbox RUN_PREFIX=`pwd`/sandbox/usr/local install-example-data
	cp -np ./misc/env.sh ./sandbox/env.sh
	. ./sandbox/env.sh && uzbl-core --uri http://www.uzbl.org --verbose
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-browser-sandbox: uzbl-browser
	make HOME=/home/sandbox DESTDIR=`pwd`/sandbox install-uzbl-browser
	make HOME=/home/sandbox DESTDIR=`pwd`/sandbox install-example-data
	cp -np ./misc/env.sh ./sandbox/env.sh
	-. ./sandbox/env.sh && uzbl-event-manager restart -avv
	. ./sandbox/env.sh && uzbl-browser --uri http://www.uzbl.org --verbose
	. ./sandbox/env.sh && uzbl-event-manager stop -ivv
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-tabbed-sandbox: uzbl-browser
	make HOME=/home/sandbox DESTDIR=`pwd`/sandbox install-example-data
	make HOME=/home/sandbox DESTDIR=`pwd`/sandbox install-uzbl-browser
	make HOME=/home/sandbox DESTDIR=`pwd`/sandbox install-uzbl-tabbed
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
	[ -d "$(DESTDIR)$(PREFIX)/bin" ] || install -d -m755 $(DESTDIR)$(PREFIX)/bin

install-uzbl-core: all install-dirs
	install -d $(DESTDIR)$(PREFIX)/share/uzbl/
	install -d $(DESTDIR)$(DOCDIR)
	install -m644 docs/* $(DESTDIR)$(DOCDIR)/
	install -m644 src/config.h $(DESTDIR)$(DOCDIR)/
	install -m644 README $(DESTDIR)$(DOCDIR)/
	install -m644 AUTHORS $(DESTDIR)$(DOCDIR)/
	install -m755 uzbl-core $(DESTDIR)$(PREFIX)/bin/uzbl-core

install-event-manager: install-dirs
	sed "s#^PREFIX = .*#PREFIX = '$(RUN_PREFIX)'#" < bin/uzbl-event-manager > $(DESTDIR)$(PREFIX)/bin/uzbl-event-manager
	chmod 755 $(DESTDIR)$(PREFIX)/bin/uzbl-event-manager

install-uzbl-browser: install-dirs install-uzbl-core install-event-manager
	sed 's#^PREFIX=.*#PREFIX=$(RUN_PREFIX)#' < bin/uzbl-browser > $(DESTDIR)$(PREFIX)/bin/uzbl-browser
	chmod 755 $(DESTDIR)$(PREFIX)/bin/uzbl-browser
	cp -r examples $(DESTDIR)$(PREFIX)/share/uzbl/
	chmod 755 $(DESTDIR)$(PREFIX)/share/uzbl/examples/data/scripts/*

install-uzbl-tabbed: install-dirs
	install -m755 bin/uzbl-tabbed $(DESTDIR)$(PREFIX)/bin/uzbl-tabbed

# you probably only want to do this manually when testing and/or to the sandbox. not meant for distributors
install-example-data:
	install -d $(DESTDIR)$(HOME)/.config/uzbl
	install -d $(DESTDIR)$(HOME)/.cache/uzbl
	install -d $(DESTDIR)$(HOME)/.local/share/uzbl
	cp -rp examples/config/* $(DESTDIR)$(HOME)/.config/uzbl/
	cp -rp examples/data/*   $(DESTDIR)$(HOME)/.local/share/uzbl/

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/bin/uzbl-*
	rm -rf $(DESTDIR)$(PREFIX)/share/uzbl
