#ifndef UZBL_MENU_H
#define UZBL_MENU_H

#include "uzbl-core.h"

typedef struct {
    gchar*   name;
    gchar*   cmd;
    gboolean issep;
    guint    context;
    gchar*   argument;
} UzblMenuItem;

void
cmd_menu_add (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_add_link (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_add_image (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_add_edit (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_add_separator (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_add_separator_link (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_add_separator_image (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_add_separator_edit (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_remove (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_remove_link (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_remove_image (WebKitWebView *view, GArray *argv, GString *result);
void
cmd_menu_remove_edit (WebKitWebView *view, GArray *argv, GString *result);

#endif
