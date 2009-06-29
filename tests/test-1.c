/* -*- c-basic-offset: 4; -*- */
#define _POSIX_SOURCE

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <webkit/webkit.h>
#include <libsoup/soup.h>
#include <JavaScriptCore/JavaScript.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <uzbl.h>
#include <config.h>

Uzbl uzbl;

static void
test_URI (void) {
    setup_scanner();
    uzbl.state.uri = g_strdup("http://www.uzbl.org");
    g_assert_cmpstr(expand_template("URI", FALSE), ==, uzbl.state.uri);
    g_free(uzbl.state.uri);
}

int
main (int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/test-1/URI", test_URI);

    return g_test_run();
}

/* vi: set et ts=4: */
