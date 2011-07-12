CPPFLAGS = -I ../ -DERRORCHECK_MUTEXES

# use GTK3-based webkit when it is available
USE_GTK3 = $(shell pkg-config --exists gtk+-3.0 webkitgtk-3.0 && echo 1)

ifeq ($(USE_GTK3),1)
	REQ_PKGS += gtk+-3.0 webkitgtk-3.0
	CPPFLAGS += -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
else
	REQ_PKGS += gtk+-2.0 webkit-1.0
	CPPFLAGS +=
endif

REQ_PKGS += libsoup-2.4 gthread-2.0 glib-2.0

ARCH:=$(shell uname -m)
ARCH!=echo `uname -m`

COMMIT_HASH:=$(shell cd .. && ./misc/hash.sh)
COMMIT_HASH!=echo `cd .. && ./misc/hash.sh`

CPPFLAGS += -DARCH=\"$(ARCH)\" -DCOMMIT=\"$(COMMIT_HASH)\"

PKG_CFLAGS:=$(shell pkg-config --cflags $(REQ_PKGS))
PKG_CFLAGS!=echo pkg-config --cflags $(REQ_PKGS)

LDLIBS:=$(shell pkg-config --libs $(REQ_PKGS) x11)
LDLIBS!=echo pkg-config --libs $(REQ_PKGS) x11

CFLAGS += -std=c99 $(PKG_CFLAGS) -ggdb -fPIC -W -Wall -Wextra -pedantic -pthread

GTESTER = gtester
GTESTER_REPORT = gtester-report

TEST_PROGS = test-expand test-command

all: $(TEST_PROGS)
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(GTESTER) --verbose $(TEST_PROGS)

${TEST_PROGS}: libuzbl-core.so

clean:
	rm -f $(TEST_PROGS)
	rm -f libuzbl-core.so
