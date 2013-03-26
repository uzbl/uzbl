#include "menu.h"

#include "util.h"
#include "uzbl-core.h"

#include <stdlib.h>
#include <string.h>

/* =========================== PUBLIC API =========================== */

static void
add_to_menu (GArray *argv, guint context);

void
cmd_menu_add (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT);
}

void
cmd_menu_add_link (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK);
}

void
cmd_menu_add_image (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
}

void
cmd_menu_add_edit (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);
}

static void
add_separator_to_menu (GArray *argv, guint context);

void
cmd_menu_add_separator (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_separator_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT);
}

void
cmd_menu_add_separator_link (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_separator_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK);
}

void
cmd_menu_add_separator_image (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_separator_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
}

void
cmd_menu_add_separator_edit (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    add_separator_to_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);
}

static void
remove_from_menu (GArray *argv, guint context);

void
cmd_menu_remove (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    remove_from_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT);
}

void
cmd_menu_remove_link (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    remove_from_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK);
}

void
cmd_menu_remove_image (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    remove_from_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
}

void
cmd_menu_remove_edit (GArray *argv, GString *result)
{
    UZBL_UNUSED (result);

    remove_from_menu (argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

void
add_to_menu (GArray *argv, guint context)
{
    UzblMenuItem *m = NULL;
    gchar *item_cmd = NULL;

    ARG_CHECK (argv, 1);

    gchar **split = g_strsplit (argv_idx (argv, 0), "=", 2);
    if (!uzbl.gui.menu_items) {
        uzbl.gui.menu_items = g_ptr_array_new ();
    }

    if (split[1]) {
        item_cmd = split[1];
    }

    if (split[0]) {
        m = (UzblMenuItem *)malloc (sizeof (UzblMenuItem));
        m->name    = g_strdup (split[0]);
        m->cmd     = g_strdup (item_cmd ? item_cmd : "");
        m->context = context;
        m->issep   = FALSE;

        g_ptr_array_add (uzbl.gui.menu_items, m);
    }

    g_strfreev (split);
}

void
add_separator_to_menu (GArray *argv, guint context)
{
    UzblMenuItem *m;
    gchar *sep_name;

    ARG_CHECK (argv, 1);

    if (!uzbl.gui.menu_items) {
        uzbl.gui.menu_items = g_ptr_array_new ();
    }

    sep_name = argv_idx (argv, 0);

    m = (UzblMenuItem *)malloc (sizeof (UzblMenuItem));
    m->name    = g_strdup (sep_name);
    m->cmd     = NULL;
    m->context = context;
    m->issep   = TRUE;

    g_ptr_array_add (uzbl.gui.menu_items, m);
}

void
remove_from_menu (GArray *argv, guint context)
{
    UzblMenuItem *mi = NULL;
    gchar *name = NULL;
    guint i = 0;

    if (!uzbl.gui.menu_items)
        return;

    ARG_CHECK (argv, 1);

    name = argv_idx (argv, 0);

    for (i = 0; i < uzbl.gui.menu_items->len; ++i) {
        mi = g_ptr_array_index (uzbl.gui.menu_items, i);

        if ((context == mi->context) && !strcmp (name, mi->name)) {
            g_free (mi->name);
            g_free (mi->cmd);
            g_free (mi);

            g_ptr_array_remove_index (uzbl.gui.menu_items, i);
        }
    }
}
