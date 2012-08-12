# packagers, set DESTDIR to your "package directory" and PREFIX to the prefix you want to have on the end-user system
# end-users who build from source: don't care about DESTDIR, update PREFIX if you want to
# RUN_PREFIX : what the prefix is when the software is run. usually the same as PREFIX
PREFIX     ?= /usr/local
INSTALLDIR ?= $(DESTDIR)$(PREFIX)
DOCDIR     ?= $(INSTALLDIR)/share/uzbl/docs
RUN_PREFIX ?= $(PREFIX)

ENABLE_WEBKIT2 ?= auto
ENABLE_GTK3    ?= auto

PYTHON=python3
PYTHONV=$(shell $(PYTHON) --version | sed -n /[0-9].[0-9]/p)
COVERAGE=$(shell which coverage)

# --- configuration ends here ---

ifeq ($(ENABLE_WEBKIT2),auto)
ENABLE_WEBKIT2 := $(shell pkg-config --exists webkit2gtk-3.0 && echo yes)
endif

ifeq ($(ENABLE_GTK3),auto)
ENABLE_GTK3 := $(shell pkg-config --exists gtk+-3.0 && echo yes)
endif

ifeq ($(ENABLE_WEBKIT2),yes)
REQ_PKGS += 'webkit2gtk-3.0 >= 1.2.4' javascriptcoregtk-3.0
CPPFLAGS += -DUSE_WEBKIT2
# WebKit2 requires GTK3
ENABLE_GTK3    := yes
else
ifeq ($(ENABLE_GTK3),yes)
REQ_PKGS += 'webkitgtk-3.0 >= 1.2.4' javascriptcoregtk-3.0
else
REQ_PKGS += 'webkit-1.0 >= 1.2.4' javascriptcoregtk-1.0
endif
endif

ifeq ($(ENABLE_GTK3),yes)
REQ_PKGS += gtk+-3.0
CPPFLAGS += -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
else
REQ_PKGS += gtk+-2.0
endif

REQ_PKGS += 'libsoup-2.4 >= 2.30' gthread-2.0 glib-2.0

ARCH:=$(shell uname -m)

COMMIT_HASH:=$(shell ./misc/hash.sh)

CPPFLAGS += -D_BSD_SOURCE -D_POSIX_SOURCE -DARCH=\"$(ARCH)\" -DCOMMIT=\"$(COMMIT_HASH)\"

PKG_CFLAGS:=$(shell pkg-config --cflags $(REQ_PKGS))

LDLIBS:=$(shell pkg-config --libs $(REQ_PKGS) x11)

CFLAGS += -std=c99 $(PKG_CFLAGS) -ggdb -W -Wall -Wextra -pedantic -pthread

SRC  = $(wildcard src/*.c)
HEAD = $(wildcard src/*.h)
OBJ  = $(foreach obj, $(SRC:.c=.o),  $(notdir $(obj)))
LOBJ = $(foreach obj, $(SRC:.c=.lo), $(notdir $(obj)))
PY = $(wildcard uzbl/*.py uzbl/plugins/*.py)

all: uzbl-browser

VPATH:=src

${OBJ}: ${HEAD}

uzbl-core: ${OBJ}

uzbl-browser: uzbl-core uzbl-event-manager

build: ${PY}
	$(PYTHON) setup.py build

.PHONY: uzbl-event-manager
uzbl-event-manager: build

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

test-event-manager: force
	${PYTHON} -m unittest discover tests/event-manager -v

coverage-event-manager: force
	${PYTHON} ${COVERAGE} erase
	${PYTHON} ${COVERAGE} run -m unittest discover tests/event-manager
	${PYTHON} ${COVERAGE} html ${PY}
	# Hmm, I wonder what a good default browser would be
	uzbl-browser htmlcov/index.html

test-uzbl-core: uzbl-core
	./uzbl-core --uri http://www.uzbl.org --verbose

test-uzbl-browser: uzbl-browser
	./bin/uzbl-browser --uri http://www.uzbl.org --verbose

test-uzbl-core-sandbox: sandbox uzbl-core sandbox-install-uzbl-core sandbox-install-example-data
	. ./sandbox/env.sh && uzbl-core --uri http://www.uzbl.org --verbose
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-browser-sandbox: sandbox uzbl-browser sandbox-install-uzbl-browser sandbox-install-example-data
	. ./sandbox/env.sh && ${PYTHON} -S `which uzbl-event-manager` restart -navv &
	. ./sandbox/env.sh && uzbl-browser --uri http://www.uzbl.org --verbose
	. ./sandbox/env.sh && ${PYTHON} -S `which uzbl-event-manager` stop -vv -o /dev/null
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-tabbed-sandbox: sandbox uzbl-browser sandbox-install-uzbl-browser sandbox-install-uzbl-tabbed sandbox-install-example-data
	. ./sandbox/env.sh && ${PYTHON} -S `which uzbl-event-manager` restart -avv
	. ./sandbox/env.sh && uzbl-tabbed
	. ./sandbox/env.sh && ${PYTHON} -S `which uzbl-event-manager` stop -avv
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

test-uzbl-event-manager-sandbox: sandbox uzbl-browser sandbox-install-uzbl-browser sandbox-install-example-data
	. ./sandbox/env.sh && ${PYTHON} -S `which uzbl-event-manager` restart -navv
	make DESTDIR=./sandbox uninstall
	rm -rf ./sandbox/usr

clean:
	rm -f uzbl-core
	rm -f *.o
	find ./examples/ -name "*.pyc" -delete
	cd ./tests/; $(MAKE) clean
	rm -rf ./sandbox/
	$(PYTHON) setup.py clean

strip:
	@echo Stripping binary
	@strip uzbl-core
	@echo ... done.

SANDBOXOPTS=\
	DESTDIR=./sandbox\
	RUN_PREFIX=`pwd`/sandbox/usr/local\
	PYINSTALL_EXTRA='--prefix=./sandbox/usr/local --install-scripts=./sandbox/usr/local/bin'

sandbox: misc/env.sh
	mkdir -p sandbox/${PREFIX}/lib64
	cp -p misc/env.sh sandbox/env.sh
	test -e sandbox/${PREFIX}/lib || ln -s lib64 sandbox/${PREFIX}/lib

sandbox-install-uzbl-browser:
	make ${SANDBOXOPTS} install-uzbl-browser

sandbox-install-uzbl-tabbed:
	make ${SANDBOXOPTS} install-uzbl-tabbed

sandbox-install-uzbl-core:
	make ${SANDBOXOPTS} install-uzbl-core

sandbox-install-event-manager:
	make ${SANDBOXOPTS} install-event-manager

sandbox-install-example-data:
	make ${SANDBOXOPTS} install-example-data

install: install-uzbl-core install-uzbl-browser install-uzbl-tabbed

install-dirs:
	[ -d "$(INSTALLDIR)/bin" ] || install -d -m755 $(INSTALLDIR)/bin

install-uzbl-core: uzbl-core install-dirs
	install -d $(INSTALLDIR)/share/uzbl/
	install -d $(DOCDIR)
	install -m644 docs/* $(DOCDIR)/
	install -m644 src/config.h $(DOCDIR)/
	install -m644 README $(DOCDIR)/
	install -m644 AUTHORS $(DOCDIR)/
	install -m755 uzbl-core $(INSTALLDIR)/bin/uzbl-core

install-event-manager: install-dirs
	$(PYTHON) setup.py install --install-scripts=$(INSTALLDIR)/bin $(PYINSTALL_EXTRA)

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
