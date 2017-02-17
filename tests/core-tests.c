#include <glib.h>

#include "../src/uzbl-core.h"

#include "../src/setup.h"
#include "../src/commands.h"

UzblCore uzbl;

static void
test_parse_simple ()
{
    GArray *argv = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse ("toggle foo bar baz", argv);
    g_assert_nonnull (cmd);
    g_assert_cmpint (3, ==, argv->len);
    g_assert_cmpstr (g_array_index (argv, gchar*, 0), ==, "foo");
    g_assert_cmpstr (g_array_index (argv, gchar*, 1), ==, "bar");
    g_assert_cmpstr (g_array_index (argv, gchar*, 2), ==, "baz");
}

static void
test_parse_quoted ()
{
    GArray *argv = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse ("toggle foo bar 'a quoted string'", argv);
    g_assert_nonnull (cmd);
    g_assert_cmpint (3, ==, argv->len);
    g_assert_cmpstr (g_array_index (argv, gchar*, 0), ==, "foo");
    g_assert_cmpstr (g_array_index (argv, gchar*, 1), ==, "bar");
    g_assert_cmpstr (g_array_index (argv, gchar*, 2), ==, "a quoted string");
}

static void
test_parse_extra_whitespace ()
{
    GArray *argv = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse ("toggle  foo  bar  baz", argv);

    g_assert_nonnull (cmd);
    g_assert_cmpint (3, ==, argv->len);
    g_assert_cmpstr (g_array_index (argv, gchar*, 0), ==, "foo");
    g_assert_cmpstr (g_array_index (argv, gchar*, 1), ==, "bar");
    g_assert_cmpstr (g_array_index (argv, gchar*, 2), ==, "baz");
}

static void
test_parse_escaped_at ()
{
    GArray *argv = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse ("spawn \\@", argv);

    g_assert_nonnull (cmd);
    g_assert_cmpint (1, ==, argv->len);
    g_assert_cmpstr (g_array_index (argv, gchar*, 0), ==, "@");
}

static void
commands_chain_cb (GObject      *source,
                   GAsyncResult *res,
                   gpointer      data)
{
    GMainLoop *loop = (GMainLoop*) data;
    GError *err = NULL;
    GString *r = uzbl_commands_run_finish (source, res, &err);

    g_assert_null (err);
    g_assert_cmpstr (r->str, ==, "foobar");

    g_main_loop_quit (loop);
}

static void
test_commands_chain ()
{
    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    GArray *argv = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse (
        "chain 'print foo' 'print bar'", argv);

    uzbl_commands_run_async (cmd, argv, TRUE, commands_chain_cb,
                             (gpointer) loop);
    g_main_loop_run (loop);
}

static void
commands_js_cb (GObject      *source,
                   GAsyncResult *res,
                   gpointer      data)
{
    GMainLoop *loop = (GMainLoop*) data;
    GError *err = NULL;
    GString *r = uzbl_commands_run_finish (source, res, &err);

    g_assert_null (err);
    g_assert_cmpstr (r->str, ==, "foobar");

    g_main_loop_quit (loop);
}

static void
test_commands_js ()
{
    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    GArray *argv = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse (
        "js clean string '\"foo\" + \"bar\"'", argv);

    uzbl_commands_run_async (cmd, argv, TRUE, commands_js_cb,
                             (gpointer) loop);
    g_main_loop_run (loop);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    uzbl_commands_init ();
    uzbl_variables_init ();
    uzbl_io_init ();

    g_test_add_func ("/uzbl/commands/parse_simple", test_parse_simple);
    g_test_add_func ("/uzbl/commands/parse_quoted", test_parse_quoted);
    g_test_add_func ("/uzbl/commands/parse_extra_whitespace", test_parse_extra_whitespace);
    g_test_add_func ("/uzbl/commands/parse_escaped_at", test_parse_escaped_at);
    g_test_add_func ("/uzbl/commands/chain", test_commands_chain);
    g_test_add_func ("/uzbl/commands/js", test_commands_js);

    return g_test_run ();
}
