CFLAGS:=-std=c99 -I$(shell pwd)/../ -L$(shell pwd) -luzbl $(shell pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4) -ggdb -Wall -W -DARCH="\"$(shell uname -m)\"" -DG_ERRORCHECK_MUTEXES -DCOMMIT="\"$(shell git log | head -n1 | sed "s/.* //")\"" $(CPPFLAGS)
LDFLAGS:=$(shell pkg-config --libs gtk+-2.0 webkit-1.0 libsoup-2.4) -pthread $(LDFLAGS)

GTESTER:=gtester
GTESTER_REPORT:=gtester-report

TEST_PROGS:=test-1

all: $(TEST_PROGS)
	LD_LIBRARY_PATH="$LD_LIBRARY_PATH:." $(GTESTER) --verbose $(TEST_PROGS)

clean:
	rm -f $(TEST_PROGS)
	rm -f libuzbl.so
