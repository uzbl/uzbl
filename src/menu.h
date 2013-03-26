#ifndef UZBL_MENU_H
#define UZBL_MENU_H

#include "webkit.h"

typedef struct {
    gchar*   name;
    gchar*   cmd;
    gboolean issep;
    guint    context;
    gchar*   argument;
} UzblMenuItem;

void
cmd_menu_add (GArray *argv, GString *result);
void
cmd_menu_add_link (GArray *argv, GString *result);
void
cmd_menu_add_image (GArray *argv, GString *result);
void
cmd_menu_add_edit (GArray *argv, GString *result);
void
cmd_menu_add_separator (GArray *argv, GString *result);
void
cmd_menu_add_separator_link (GArray *argv, GString *result);
void
cmd_menu_add_separator_image (GArray *argv, GString *result);
void
cmd_menu_add_separator_edit (GArray *argv, GString *result);
void
cmd_menu_remove (GArray *argv, GString *result);
void
cmd_menu_remove_link (GArray *argv, GString *result);
void
cmd_menu_remove_image (GArray *argv, GString *result);
void
cmd_menu_remove_edit (GArray *argv, GString *result);

#endif
