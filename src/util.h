#include <glib.h>
#include <stdio.h>

enum xdg_type {
  XDG_CONFIG, XDG_DATA, XDG_CACHE
};

void        ensure_xdg_vars(void);
gchar*      find_xdg_file(enum xdg_type type, const char* filename);
gboolean    file_exists(const char* filename);
char*       str_replace(const char* search, const char* replace, const char* string);
gboolean    for_each_line_in_file(const gchar *path, void (*callback)(const gchar *l, void *c), void *user_data);
gchar*      find_existing_file(const gchar*);
gchar*      argv_idx(const GArray*, const guint);

/**
 * appends `src' to `dest' with backslash, single-quotes and newlines in
 * `src' escaped
 */
GString *   append_escaped (GString *dest, const gchar *src);

void        sharg_append (GArray *array, const gchar *str);
