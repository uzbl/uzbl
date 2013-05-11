#ifndef UZBL_MENU_H
#define UZBL_MENU_H

#include <glib.h>

typedef struct {
    gchar*   name;
    gchar*   cmd;
    gboolean issep;
    guint    context;
    gchar*   argument;
} UzblMenuItem;

#endif
