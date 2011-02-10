#include <glib.h>
#include <stdio.h>

typedef struct {
    gchar* environmental;
    gchar* default_value;
} XDG_Var;


gchar*      get_xdg_var(XDG_Var xdg);
gchar*      find_xdg_file(int xdg_type, const char* filename);
gboolean    file_exists(const char* filename);
char*       str_replace(const char* search, const char* replace, const char* string);
gboolean    for_each_line_in_file(const gchar *path, void (*callback)(const gchar *l, void *c), void *user_data);
char*       itos(int val);
gchar*      strfree(gchar *str);
gchar*      argv_idx(const GArray*, const guint);
