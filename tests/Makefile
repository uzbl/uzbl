# gtk2
REQ_PKGS = gtk+-2.0 webkit-1.0
CPPFLAGS = -DERRORCHECK_MUTEXES -I ../

# gtk3
#REQ_PKGS = gtk+-3.0 webkitgtk-3.0
#CPPFLAGS	= -DERRORCHECK_MUTEXES -DGTK3 -I ../

# --- configuration ends here ---

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
