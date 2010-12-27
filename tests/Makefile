REQ_PKGS  = libsoup-2.4 gthread-2.0 glib-2.0

# gtk2
REQ_PKGS += gtk+-2.0 webkit-1.0
CPPFLAGS  =

# gtk3
#REQ_PKGS += gtk+-3.0 webkitgtk-3.0
#CPPFLAGS	 = -DGTK3

CFLAGS:=-std=c99 -I$(shell pwd)/../ -L$(shell pwd) -luzbl-core $(shell pkg-config --cflags $(REQ_PKGS)) -ggdb -Wall -W -DARCH="\"$(shell uname -m)\"" -lgthread-2.0 -DG_ERRORCHECK_MUTEXES -DCOMMIT="\"$(shell git log | head -n1 | sed "s/.* //")\"" $(CPPFLAGS)
CFLAGS!=echo -std=c99 `pkg-config --cflags $(REQ_PKGS)` -ggdb -Wall -W -DARCH='"\""'`uname -m`'"\""' -lgthread-2.0 -DG_ERRORCHECK_MUTEXES -DCOMMIT='"\""'`git log | head -n1 | sed "s/.* //"`'"\""' $(CPPFLAGS)

LDFLAGS:=$(shell pkg-config --libs $(REQ_PKGS)) -pthread $(LDFLAGS)
LDFLAGS!=echo `pkg-config --libs $(REQ_PKGS)` -pthread $(LDFLAGS)

GTESTER:=gtester
GTESTER_REPORT:=gtester-report

TEST_PROGS:=test-expand test-command

all: $(TEST_PROGS)
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(GTESTER) --verbose $(TEST_PROGS)

clean:
	rm -f $(TEST_PROGS)
	rm -f libuzbl-core.so
