#include <glib.h>
#include <stdio.h>

typedef struct {
    gchar* environmental;
    gchar* default_value;
} XDG_Var;

enum exp_type {EXP_ERR, EXP_SIMPLE_VAR, EXP_BRACED_VAR, EXP_EXPR, EXP_JS, EXP_ESCAPE};


gchar*      get_xdg_var(XDG_Var xdg);
gchar*      find_xdg_file(int xdg_type, const char* filename);
gboolean    file_exists(const char* filename);
char*       str_replace(const char* search, const char* replace, const char* string);
gboolean    for_each_line_in_file(const gchar *path, void (*callback)(const gchar *l, void *c), void *user_data);
enum exp_type get_exp_type(const gchar*);
gchar*      find_existing_file(const gchar*);
gchar*      argv_idx(const GArray*, const guint);
