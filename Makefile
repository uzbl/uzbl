all:
	gcc `pkg-config --libs --cflags gtk+-2.0` `pkg-config --libs --cflags webkit-1.0` `pkg-config --libs --cflags glib-2.0` -Wall uzbl.c -o uzbl
test:
	./uzbl --uri http://www.archlinux.org
