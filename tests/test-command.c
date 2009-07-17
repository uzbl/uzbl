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

extern Uzbl uzbl;

void
test_keycmd (void) {
  add_binding("insert", "set insert_mode = 1");
  add_binding("command", "set insert_mode = 0");

  /* the 'keycmd' command */
  parse_command("keycmd", "insert", NULL);

  g_assert_cmpint(1, ==, uzbl.behave.insert_mode);
  g_assert_cmpstr("", ==, uzbl.state.keycmd);

  /* setting the keycmd variable directly, equivalent to the 'keycmd' comand */
  set_var_value("keycmd", "command");

  g_assert_cmpint(0, ==, uzbl.behave.insert_mode);
  g_assert_cmpstr("", ==, uzbl.state.keycmd);
}

int
main (int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/test-command/keycmd", test_keycmd);

    initialize(argc, argv);

    return g_test_run();
}

/* vi: set et ts=4: */
