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

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    uzbl_commands_init ();

    g_test_add_func ("/uzbl/commands/parse_simple", test_parse_simple);
    g_test_add_func ("/uzbl/commands/parse_quoted", test_parse_quoted);
    g_test_add_func ("/uzbl/commands/parse_extra_whitespace", test_parse_extra_whitespace);

    return g_test_run ();
}
