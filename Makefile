CFLAGS:=-std=c99 $(shell pkg-config --cflags gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0) -ggdb -Wall -W -DARCH="\"$(shell uname -m)\"" -lgthread-2.0 -DG_ERRORCHECK_MUTEXES -DCOMMIT="\"$(shell git log | head -n1 | sed "s/.* //")\"" $(CPPFLAGS)
LDFLAGS:=$(shell pkg-config --libs gtk+-2.0 webkit-1.0 libsoup-2.4 gthread-2.0) -pthread $(LDFLAGS)
all: uzbl uzblctrl

PREFIX?=$(DESTDIR)/usr

# When compiling unit tests, compile uzbl as a library first
test: uzbl.o
	$(CC) -DUZBL_LIBRARY -shared -Wl uzbl.o -o ./tests/libuzbl.so
	cd ./tests/; $(MAKE)

# test-report: run tests in subdirs and generate report
# perf-report: run tests in subdirs with -m perf and generate report
# full-report: like test-report: with -m perf and -m slow
#test-report perf-report full-report:    ${TEST_PROGS}
#	@test -z "${TEST_PROGS}" || { \
#          case $@ in \
#          test-report) test_options="-k";; \
#          perf-report) test_options="-k -m=perf";; \
#          full-report) test_options="-k -m=perf -m=slow";; \
#          esac ; \
#          if test -z "$$GTESTER_LOGDIR" ; then  \
#            ${GTESTER} --verbose $$test_options -o test-report.xml ${TEST_PROGS} ; \
#          elif test -n "${TEST_PROGS}" ; then \
#            ${GTESTER} --verbose $$test_options -o `mktemp "$$GTESTER_LOGDIR/log-XXXXXX"` ${TEST_PROGS} ; \
#          fi ; \
#        }
#	@ ignore_logdir=true ; \
#          if test -z "$$GTESTER_LOGDIR" ; then \
#            GTESTER_LOGDIR=`mktemp -d "\`pwd\`/.testlogs-XXXXXX"`; export GTESTER_LOGDIR ; \
#            ignore_logdir=false ; \
#          fi ; \
#          for subdir in $(SUBDIRS) . ; do \
#            test "$$subdir" = "." -o "$$subdir" = "po" || \
#            ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
#          done ; \
#          $$ignore_logdir || { \
#            echo '<?xml version="1.0"?>' > $@.xml ; \
#            echo '<report-collection>'  >> $@.xml ; \
#            for lf in `ls -L "$$GTESTER_LOGDIR"/.` ; do \
#              sed '1,1s/^<?xml\b[^>?]*?>//' <"$$GTESTER_LOGDIR"/"$$lf" >> $@.xml ; \
#            done ; \
#            echo >> $@.xml ; \
#            echo '</report-collection>' >> $@.xml ; \
#            rm -rf "$$GTESTER_LOGDIR"/ ; \
#            ${GTESTER_REPORT} --version 2>/dev/null 1>&2 ; test "$$?" != 0 || ${GTESTER_REPORT} $@.xml >$@.html ; \
#          }

test-dev: uzbl
	XDG_DATA_HOME=./examples/data               XDG_CONFIG_HOME=./examples/config               ./uzbl --uri http://www.uzbl.org --verbose

test-share: uzbl
	XDG_DATA_HOME=/usr/share/uzbl/examples/data XDG_CONFIG_HOME=/usr/share/uzbl/examples/config ./uzbl --uri http://www.uzbl.org --verbose

	
clean:
	rm -f uzbl
	rm -f uzblctrl
	rm -f uzbl.o
	cd ./tests/; $(MAKE) clean

install:
	install -d $(PREFIX)/bin
	install -d $(PREFIX)/share/uzbl/docs
	install -d $(PREFIX)/share/uzbl/examples
	install -m755 uzbl $(PREFIX)/bin/uzbl
	install -m755 uzblctrl $(PREFIX)/bin/uzblctrl
	cp -rp docs     $(PREFIX)/share/uzbl/
	cp -rp config.h $(PREFIX)/share/uzbl/docs/
	cp -rp examples $(PREFIX)/share/uzbl/
	install -m644 AUTHORS $(PREFIX)/share/uzbl/docs
	install -m644 README  $(PREFIX)/share/uzbl/docs


uninstall:
	rm -rf $(PREFIX)/bin/uzbl
	rm -rf $(PREFIX)/bin/uzblctrl
	rm -rf $(PREFIX)/share/uzbl
