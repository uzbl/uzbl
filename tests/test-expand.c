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

extern gchar* expand(char*, guint, gboolean);
extern gchar* expand_template(const char*, gboolean);
extern void make_var_to_name_hash(void);

void
test_URI (void) {
    uzbl.state.uri = g_strdup("http://www.uzbl.org/");
    g_assert_cmpstr(expand_template("URI", FALSE), ==, uzbl.state.uri);
    g_free(uzbl.state.uri);
}

void
test_LOAD_PROGRESS (void) {
    uzbl.gui.sbar.load_progress = 50;
    g_assert_cmpstr(expand_template("LOAD_PROGRESS", FALSE), ==, "50");
}

void
test_LOAD_PROGRESSBAR (void) {
    uzbl.gui.sbar.load_progress = 75;
    uzbl.gui.sbar.progress_w = 4;

    uzbl.gui.sbar.progress_s = "*";
    uzbl.gui.sbar.progress_u = "-";

    g_assert_cmpstr(expand_template("LOAD_PROGRESSBAR", FALSE), ==, "***-");
}

void
test_TITLE (void) {
    uzbl.gui.main_title = "Lorem Ipsum";
    g_assert_cmpstr(expand_template("TITLE", FALSE), ==, "Lorem Ipsum");
}

void
test_SELECTED_URI (void) {
    uzbl.state.selected_url = "http://example.org/";
    g_assert_cmpstr(expand_template("SELECTED_URI", FALSE), ==, "http://example.org/");
}

void
test_NAME (void) {
    uzbl.state.instance_name = "12345";
    g_assert_cmpstr(expand_template("NAME", FALSE), ==, "12345");
}

void
test_KEYCMD (void) {
    uzbl.state.keycmd = g_string_new("gg winslow");
    g_assert_cmpstr(expand_template("KEYCMD", FALSE), ==, "gg winslow");
    g_string_free(uzbl.state.keycmd, TRUE);
}

void
test_MODE (void) {
    uzbl.behave.cmd_indicator = "C";
    uzbl.behave.insert_indicator = "I";

    uzbl.behave.insert_mode = 0;
    g_assert_cmpstr(expand_template("MODE", FALSE), ==, "C");

    uzbl.behave.insert_mode = 1;
    g_assert_cmpstr(expand_template("MODE", FALSE), ==, "I");
}

void
test_MSG (void) {
    uzbl.gui.sbar.msg = "Hello from frosty Edmonton!";
    g_assert_cmpstr(expand_template("MSG", FALSE), ==, "Hello from frosty Edmonton!");
}

void
test_WEBKIT_VERSION (void) {
    GString* expected = g_string_new("");
    g_string_append(expected, itos(WEBKIT_MAJOR_VERSION));
    g_string_append(expected, " ");
    g_string_append(expected, itos(WEBKIT_MINOR_VERSION));
    g_string_append(expected, " ");
    g_string_append(expected, itos(WEBKIT_MICRO_VERSION));

    g_assert_cmpstr(expand("@WEBKIT_MAJOR @WEBKIT_MINOR @WEBKIT_MICRO", 0, FALSE), ==, g_string_free(expected, FALSE));
}

void
test_ARCH_UZBL (void) {
    g_assert_cmpstr(expand("@ARCH_UZBL", 0, FALSE), ==, ARCH);
}

void
test_COMMIT (void) {
    g_assert_cmpstr(expand("@COMMIT", 0, FALSE), ==, COMMIT);
}

void
test_cmd_useragent_simple (void) {
    GString* expected = g_string_new("Uzbl (Webkit ");
    g_string_append(expected, itos(WEBKIT_MAJOR_VERSION));
    g_string_append(expected, ".");
    g_string_append(expected, itos(WEBKIT_MINOR_VERSION));
    g_string_append(expected, ".");
    g_string_append(expected, itos(WEBKIT_MICRO_VERSION));
    g_string_append(expected, ")");

    set_var_value("useragent", "Uzbl (Webkit @WEBKIT_MAJOR.@WEBKIT_MINOR.@WEBKIT_MICRO)");
    g_assert_cmpstr(uzbl.net.useragent, ==, g_string_free(expected, FALSE));
}

void
test_cmd_useragent_full (void) {
    GString* expected = g_string_new("Uzbl (Webkit ");
    g_string_append(expected, itos(WEBKIT_MAJOR_VERSION));
    g_string_append(expected, ".");
    g_string_append(expected, itos(WEBKIT_MINOR_VERSION));
    g_string_append(expected, ".");
    g_string_append(expected, itos(WEBKIT_MICRO_VERSION));
    g_string_append(expected, ") (");

    struct utsname unameinfo;
    if(uname(&unameinfo) == -1)
      g_printerr("Can't retrieve unameinfo. This test might fail.\n");

    g_string_append(expected, unameinfo.sysname);
    g_string_append(expected, " ");
    g_string_append(expected, unameinfo.nodename);
    g_string_append(expected, " ");
    g_string_append(expected, unameinfo.release);
    g_string_append(expected, " ");
    g_string_append(expected, unameinfo.version);
    g_string_append(expected, " ");
    g_string_append(expected, unameinfo.machine);
    g_string_append(expected, " [");
    g_string_append(expected, ARCH);
    g_string_append(expected, "]) (Commit ");
    g_string_append(expected, COMMIT);
    g_string_append(expected, ")");

    set_var_value("useragent", "Uzbl (Webkit @WEBKIT_MAJOR.@WEBKIT_MINOR.@WEBKIT_MICRO) (@(uname -s)@ @(uname -n)@ @(uname -r)@ @(uname -v)@ @(uname -m)@ [@ARCH_UZBL]) (Commit @COMMIT)");
    g_assert_cmpstr(uzbl.net.useragent, ==, g_string_free(expected, FALSE));
}

void
test_escape_markup (void) {
    /* simple expansion */
    uzbl.state.uri = g_strdup("<&>");
    g_assert_cmpstr(expand("@uri", 0, FALSE), ==, uzbl.state.uri);
    g_assert_cmpstr(expand("@uri", 0, TRUE), ==, "&lt;&amp;&gt;");

    /* shell expansion */
    g_assert_cmpstr(expand("@(echo -n '<&>')@", 0, FALSE), ==, "<&>");
    g_assert_cmpstr(expand("@(echo -n '<&>')@", 0, TRUE), ==, "&lt;&amp;&gt;");

    /* javascript expansion */
    g_assert_cmpstr(expand("@<'<&>'>@", 0, FALSE), ==, "<&>");
    g_assert_cmpstr(expand("@<'<&>'>@", 0, TRUE), ==, "&lt;&amp;&gt;");

    g_free(uzbl.state.uri);
}

int
main (int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/test-expand/URI", test_URI);
    g_test_add_func("/test-expand/LOAD_PROGRESS", test_LOAD_PROGRESS);
    g_test_add_func("/test-expand/LOAD_PROGRESSBAR", test_LOAD_PROGRESSBAR);
    g_test_add_func("/test-expand/TITLE", test_TITLE);
    g_test_add_func("/test-expand/SELECTED_URI", test_SELECTED_URI);
    g_test_add_func("/test-expand/NAME", test_NAME);
    g_test_add_func("/test-expand/KEYCMD", test_KEYCMD);
    g_test_add_func("/test-expand/MODE", test_MODE);
    g_test_add_func("/test-expand/MSG", test_MSG);
    g_test_add_func("/test-expand/WEBKIT_VERSION", test_WEBKIT_VERSION);
    g_test_add_func("/test-expand/ARCH_UZBL", test_ARCH_UZBL);
    g_test_add_func("/test-expand/COMMIT", test_COMMIT);

    g_test_add_func("/test-expand/cmd_useragent_simple", test_cmd_useragent_simple);
    g_test_add_func("/test-expand/cmd_useragent_full", test_cmd_useragent_full);

    g_test_add_func("/test-expand/escape_markup", test_escape_markup);

    initialize(argc, argv);

    return g_test_run();
}

/* vi: set et ts=4: */
