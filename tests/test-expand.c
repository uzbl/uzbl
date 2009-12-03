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

#include <uzbl-core.h>
#include <config.h>

extern UzblCore uzbl;

extern gchar* expand(char*, guint);
extern void make_var_to_name_hash(void);

void
test_keycmd (void) {
    uzbl.state.keycmd = "gg winslow";
    g_assert_cmpstr(expand("@keycmd", 0), ==, "gg winslow");
}

void
test_uri (void) {
    g_assert_cmpstr(expand("@uri", 0), ==, "");

    uzbl.state.uri = g_strdup("http://www.uzbl.org/");
    g_assert_cmpstr(expand("@uri", 0), ==, uzbl.state.uri);
    g_free(uzbl.state.uri);
}

void
test_TITLE (void) {
    uzbl.gui.main_title = "Lorem Ipsum";
    g_assert_cmpstr(expand("@TITLE", 0), ==, "Lorem Ipsum");
}

void
test_SELECTED_URI (void) {
    uzbl.state.selected_url = "http://example.org/";
    g_assert_cmpstr(expand("@SELECTED_URI", 0), ==, "http://example.org/");
}

void
test_NAME (void) {
    uzbl.state.instance_name = "testing";
    g_assert_cmpstr(expand("@NAME", 0), ==, "testing");
}

void
test_useragent (void) {
    uzbl.net.useragent = "This is the uzbl browser (sort of).  and btw: Hello from frosty Edmonton!";
    g_assert_cmpstr(expand("@useragent", 0), ==, "This is the uzbl browser (sort of).  and btw: Hello from frosty Edmonton!");
}

void
test_WEBKIT_VERSION (void) {
    GString* expected = g_string_new("");
    g_string_append(expected, itos(WEBKIT_MAJOR_VERSION));
    g_string_append(expected, " ");
    g_string_append(expected, itos(WEBKIT_MINOR_VERSION));
    g_string_append(expected, " ");
    g_string_append(expected, itos(WEBKIT_MICRO_VERSION));

    g_assert_cmpstr(expand("@WEBKIT_MAJOR @WEBKIT_MINOR @WEBKIT_MICRO", 0), ==, g_string_free(expected, FALSE));
}

void
test_ARCH_UZBL (void) {
    g_assert_cmpstr(expand("@ARCH_UZBL", 0), ==, ARCH);
}

void
test_COMMIT (void) {
    g_assert_cmpstr(expand("@COMMIT", 0), ==, uzbl.info.commit);
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

    g_assert_cmpstr(expand("Uzbl (Webkit @WEBKIT_MAJOR.@WEBKIT_MINOR.@WEBKIT_MICRO)", 0), ==, g_string_free(expected, FALSE));
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
    g_string_append(expected, uzbl.info.commit);
    g_string_append(expected, ")");

    g_assert_cmpstr(expand("Uzbl (Webkit @WEBKIT_MAJOR.@WEBKIT_MINOR.@WEBKIT_MICRO) (@(uname -s)@ @(uname -n)@ @(uname -r)@ @(uname -v)@ @(uname -m)@ [@ARCH_UZBL]) (Commit @COMMIT)", 0), ==, g_string_free(expected, FALSE));
}

void
test_escape_markup (void) {
    /* simple expansion */
    uzbl.state.uri = g_strdup("<&>");
    g_assert_cmpstr(expand("@uri", 0), ==, uzbl.state.uri);
    g_assert_cmpstr(expand("@[@uri]@", 0), ==, "&lt;&amp;&gt;");

    /* shell expansion */
    g_assert_cmpstr(expand("@(echo -n '<&>')@", 0), ==, "<&>");
    g_assert_cmpstr(expand("@[@(echo -n '<&>')@]@", 0), ==, "&lt;&amp;&gt;");

    /* javascript expansion */
    g_assert_cmpstr(expand("@<'<&>'>@", 0), ==, "<&>");
    g_assert_cmpstr(expand("@[@<'<&>'>@]@", 0), ==, "&lt;&amp;&gt;");

    g_free(uzbl.state.uri);
}

void
test_escape_expansion (void) {
    /* \@ -> @ */
    g_assert_cmpstr(expand("\\@uri", 0), ==, "@uri");

    /* \\\@ -> \@ */
    g_assert_cmpstr(expand("\\\\\\@uri", 0), ==, "\\@uri");

    /* \@(...)\@ -> @(...)@ */
    g_assert_cmpstr(expand("\\@(echo hi)\\@", 0), ==, "@(echo hi)@");

    /* \@<...>\@ -> @<...>@ */
    g_assert_cmpstr(expand("\\@<\"hi\">\\@", 0), ==, "@<\"hi\">@");
}

void
test_nested (void) {
    uzbl.net.useragent = "xxx";
    g_assert_cmpstr(expand("@<\"..@useragent..\">@", 0), ==, "..xxx..");
    g_assert_cmpstr(expand("@<\"..\\@useragent..\">@", 0), ==, "..@useragent..");

    g_assert_cmpstr(expand("@(echo ..@useragent..)@", 0), ==, "..xxx..");
    g_assert_cmpstr(expand("@(echo ..\\@useragent..)@", 0), ==, "..@useragent..");
}

int
main (int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/test-expand/@keycmd", test_keycmd);
    g_test_add_func("/test-expand/@useragent", test_useragent);
    g_test_add_func("/test-expand/@uri", test_uri);
    g_test_add_func("/test-expand/@TITLE", test_TITLE);
    g_test_add_func("/test-expand/@SELECTED_URI", test_SELECTED_URI);
    g_test_add_func("/test-expand/@NAME", test_NAME);
    g_test_add_func("/test-expand/@WEBKIT_*", test_WEBKIT_VERSION);
    g_test_add_func("/test-expand/@ARCH_UZBL", test_ARCH_UZBL);
    g_test_add_func("/test-expand/@COMMIT", test_COMMIT);

    g_test_add_func("/test-expand/cmd_useragent_simple", test_cmd_useragent_simple);
    g_test_add_func("/test-expand/cmd_useragent_full", test_cmd_useragent_full);

    g_test_add_func("/test-expand/escape_markup", test_escape_markup);
    g_test_add_func("/test-expand/escape_expansion", test_escape_expansion);
    g_test_add_func("/test-expand/nested", test_nested);

    initialize(argc, argv);

    return g_test_run();
}

/* vi: set et ts=4: */
