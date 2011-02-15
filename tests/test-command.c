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

#include <src/uzbl-core.h>
#include <src/config.h>

extern UzblCore uzbl;

#define INSTANCE_NAME "testing"

#define ASSERT_EVENT(EF, STR) { read_event(ef); \
    g_assert_cmpstr("EVENT [" INSTANCE_NAME "] " STR "\n", ==, ef->event_buffer); }

struct EventFixture
{
  /* uzbl's end of the socketpair */
  int uzbl_sock;

  /* the test framework's end of the socketpair */
  int test_sock;
  char event_buffer[1024];
};

void
read_event (struct EventFixture *ef) {
    int r = read(ef->test_sock, ef->event_buffer, 1023); \
    ef->event_buffer[r] = 0;
}

void
assert_no_event (struct EventFixture *ef) {
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(ef->test_sock, &rfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    /* check if there's any data waiting */
    int res = select(ef->test_sock + 1, &rfds, NULL, NULL, &timeout);

    if(res == 0) {
        /* timeout expired, there was no event */

        /* success */
        return;
    } else if(res == -1) {
        /* mechanical failure */
        perror("select():");
        assert(0);
    } else {
        /* there was an event. display it. */
        read_event(ef);
        g_assert_cmpstr("", ==, ef->event_buffer);
    }
}

void
event_fixture_setup(struct EventFixture *ef, const void* data)
{
    (void) data;

    int socks[2];

    /* make some sockets, fresh for every test */
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, socks) == -1)
    {
      perror("socketpair() failed");
      g_assert(0);
    }

    ef->uzbl_sock = socks[0];
    ef->test_sock = socks[1];

    /* attach uzbl_sock to uzbl's event dispatcher. */
    GIOChannel *iochan = g_io_channel_unix_new(ef->uzbl_sock);
    g_io_channel_set_encoding(iochan, NULL, NULL);

    if(!uzbl.comm.connect_chan)
        uzbl.comm.connect_chan = g_ptr_array_new();
    if(!uzbl.comm.client_chan)
        uzbl.comm.client_chan = g_ptr_array_new();
    g_ptr_array_add(uzbl.comm.client_chan, (gpointer)iochan);
}

void
event_fixture_teardown(struct EventFixture *ef, const void *data)
{
    (void) data;

    /* there should be no events left waiting */
    assert_no_event(ef);

    /* clean up the io channel we opened for uzbl */
    GIOChannel *iochan = g_ptr_array_index(uzbl.comm.client_chan, 0);
    remove_socket_from_array(iochan);

    /* close the sockets so that nothing sticks around between tests */
    close(ef->uzbl_sock);
    close(ef->test_sock);
}

/* actual tests begin here */

void
test_event (struct EventFixture *ef, const void *data) {
    (void) data;

    parse_cmd_line("event", NULL);
    assert_no_event(ef);

    /* a simple event can be sent */
    parse_cmd_line("event event_type arg u ments", NULL);
    ASSERT_EVENT(ef, "EVENT_TYPE arg u ments");

    /* arguments to event should be expanded */
    parse_cmd_line("event event_type @(echo expansion)@ test", NULL);
    ASSERT_EVENT(ef, "EVENT_TYPE expansion test");

    /* "request" is just an alias for "event" */
    parse_cmd_line("request event_type arg u ments", NULL);
    ASSERT_EVENT(ef, "EVENT_TYPE arg u ments");
}


void
test_set_variable (struct EventFixture *ef, const void *data) {
    (void) data;

    /* set a string */
    parse_cmd_line("set useragent = Uzbl browser kthxbye!", NULL);
    ASSERT_EVENT(ef, "VARIABLE_SET useragent str Uzbl browser kthxbye!");
    g_assert_cmpstr("Uzbl browser kthxbye!", ==, uzbl.net.useragent);

    /* set an int */
    parse_cmd_line("set forward_keys = 0", NULL);
    ASSERT_EVENT(ef, "VARIABLE_SET forward_keys int 0");
    g_assert_cmpint(0, ==, uzbl.behave.forward_keys);

    /* set a float */
    /* we have to be careful about locales here */
    GString *cmd, *ev;
    cmd = g_string_new("set zoom_level = ");
    g_string_append_printf(cmd, "%f", 0.25);
    parse_cmd_line(g_string_free(cmd, FALSE), NULL);

    ev = g_string_new("EVENT [" INSTANCE_NAME "] VARIABLE_SET zoom_level float ");
    g_string_append_printf(ev, "%f\n", 0.25);
    read_event(ef);
    g_assert_cmpstr(g_string_free(ev, FALSE), ==, ef->event_buffer);

    g_assert_cmpfloat(0.25, ==, uzbl.behave.zoom_level);

    /* set a constant int (nothing should happen) */
    int old_major = uzbl.info.webkit_major;
    parse_cmd_line("set WEBKIT_MAJOR = 100", NULL);
    assert_no_event(ef);
    g_assert_cmpint(old_major, ==, uzbl.info.webkit_major);

    /* set a constant str (nothing should happen)  */
    GString *old_arch = g_string_new(uzbl.info.arch);
    parse_cmd_line("set ARCH_UZBL = A Lisp Machine", NULL);
    assert_no_event(ef);
    g_assert_cmpstr(g_string_free(old_arch, FALSE), ==, uzbl.info.arch);

    /* set a custom variable */
    parse_cmd_line("set nonexistant_variable = Some Value", NULL);
    ASSERT_EVENT(ef, "VARIABLE_SET nonexistant_variable str Some Value");
    uzbl_cmdprop *c = g_hash_table_lookup(uzbl.comm.proto_var, "nonexistant_variable");
    g_assert_cmpstr("Some Value", ==, *c->ptr.s);

    /* set a custom variable with expansion */
    parse_cmd_line("set an_expanded_variable = Test @(echo expansion)@", NULL);
    ASSERT_EVENT(ef, "VARIABLE_SET an_expanded_variable str Test expansion");
    c = g_hash_table_lookup(uzbl.comm.proto_var, "an_expanded_variable");
    g_assert_cmpstr("Test expansion", ==, *c->ptr.s);
}

void
test_print (void) {
    GString *result = g_string_new("");

    /* a simple message can be returned as a result */
    parse_cmd_line("print A simple test", result);
    g_assert_cmpstr("A simple test", ==, result->str);

    /* arguments to print should be expanded */
    parse_cmd_line("print A simple @(echo expansion)@ test", result);
    g_assert_cmpstr("A simple expansion test", ==, result->str);

    g_string_free(result, TRUE);
}

void
test_scroll (void) {
    uzbl.gui.scbar_v = (GtkScrollbar*) gtk_vscrollbar_new (NULL);
    uzbl.gui.bar_v = gtk_range_get_adjustment((GtkRange*) uzbl.gui.scbar_v);

    gtk_adjustment_set_lower(uzbl.gui.bar_v, 0);
    gtk_adjustment_set_upper(uzbl.gui.bar_v, 100);
    gtk_adjustment_set_page_size(uzbl.gui.bar_v, 5);

    /* scroll vertical end should scroll it to upper - page_size */
    parse_cmd_line("scroll vertical end", NULL);
    g_assert_cmpfloat(gtk_adjustment_get_value(uzbl.gui.bar_v), ==, 95);

    /* scroll vertical begin should scroll it to lower */
    parse_cmd_line("scroll vertical begin", NULL);
    g_assert_cmpfloat(gtk_adjustment_get_value(uzbl.gui.bar_v), ==, 0);

    /* scroll vertical can scroll by pixels */
    parse_cmd_line("scroll vertical 15", NULL);
    g_assert_cmpfloat(gtk_adjustment_get_value(uzbl.gui.bar_v), ==, 15);

    parse_cmd_line("scroll vertical -10", NULL);
    g_assert_cmpfloat(gtk_adjustment_get_value(uzbl.gui.bar_v), ==, 5);

    /* scroll vertical can scroll by a percentage of the page size */
    parse_cmd_line("scroll vertical 100%", NULL);
    g_assert_cmpfloat(gtk_adjustment_get_value(uzbl.gui.bar_v), ==, 10);

    parse_cmd_line("scroll vertical 150%", NULL);
    g_assert_cmpfloat(gtk_adjustment_get_value(uzbl.gui.bar_v), ==, 17.5);

    /* scroll_horz behaves basically the same way. */
}

void
test_toggle_status (void) {
    g_assert(!uzbl.behave.show_status);

    /* status bar can be toggled on */
    parse_cmd_line("toggle_status", NULL);
    g_assert(uzbl.behave.show_status);

    /* status bar can be toggled back off */
    parse_cmd_line("toggle_status", NULL);
    g_assert(!uzbl.behave.show_status);
}

void
test_sync_sh (void) {
    parse_cmd_line("sync_sh 'echo Test echo.'", NULL);
    g_assert_cmpstr("Test echo.\n", ==, uzbl.comm.sync_stdout);

    /* clean up after ourselves */
    uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);
}

void
test_js (void) {
    GString *result = g_string_new("");

    /* simple javascript can be evaluated and returned */
    parse_cmd_line("js ('x' + 345).toUpperCase()", result);
    g_assert_cmpstr("X345", ==, result->str);

    g_string_free(result, TRUE);
}

void
test_run_handler_arg_order (void) {
    run_handler("sync_spawn echo uvw xyz", "abc def");

    assert(uzbl.comm.sync_stdout);

    /* the rest of the result should be the arguments passed to run_handler. */
    /* the arguments in the second argument to run_handler should be placed before any
     * included in the first argument to run handler. */
    g_assert_cmpstr("abc def uvw xyz\n", ==, uzbl.comm.sync_stdout);
}

void
test_run_handler_expand (void) {
    uzbl.net.useragent = "Test uzbl uzr agent";
    run_handler("sync_spawn echo @useragent", "result:");

    assert(uzbl.comm.sync_stdout);

    /* the user-specified arguments to the handler should have been expanded */
    g_assert_cmpstr("result: Test uzbl uzr agent\n", ==, uzbl.comm.sync_stdout);
}

int
main (int argc, char *argv[]) {
    /* set up tests */
    g_type_init();
    g_test_init(&argc, &argv, NULL);

    g_test_add("/test-command/set-variable",   struct EventFixture, NULL, event_fixture_setup, test_set_variable, event_fixture_teardown);
    g_test_add("/test-command/event",          struct EventFixture, NULL, event_fixture_setup, test_event,        event_fixture_teardown);

    g_test_add_func("/test-command/print",          test_print);
    g_test_add_func("/test-command/scroll",         test_scroll);
    g_test_add_func("/test-command/toggle-status",  test_toggle_status);
    g_test_add_func("/test-command/sync-sh",        test_sync_sh);

    g_test_add_func("/test-command/js",             test_js);

    /* the following aren't really "command" tests, but they're not worth
     * splitting into a separate file yet */
    g_test_add_func("/test-command/run_handler/arg-order",      test_run_handler_arg_order);
    g_test_add_func("/test-command/run_handler/expand",         test_run_handler_expand);

    /* set up uzbl */
    initialize(argc, argv);

    uzbl.state.config_file = "/tmp/uzbl-config";
    uzbl.comm.fifo_path = "/tmp/some-nonexistant-fifo";
    uzbl.comm.socket_path = "/tmp/some-nonexistant-socket";
    uzbl.state.uri = "http://example.org/";
    uzbl.gui.main_title = "Example.Org";

    uzbl.state.instance_name = INSTANCE_NAME;
    uzbl.behave.shell_cmd = "sh -c";

    return g_test_run();
}

/* vi: set et ts=4: */
