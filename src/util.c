#define _POSIX_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "util.h"

gchar* find_existing_file2(gchar *, const gchar *);

const XDG_Var XDG[] = {
    { "XDG_CONFIG_HOME", "~/.config" },
    { "XDG_DATA_HOME",   "~/.local/share" },
    { "XDG_CACHE_HOME",  "~/.cache" },
    { "XDG_CONFIG_DIRS", "/etc/xdg" },
    { "XDG_DATA_DIRS",   "/usr/local/share/:/usr/share/" },
};

/*@null@*/ gchar*
get_xdg_var (XDG_Var xdg) {
    const gchar *actual_value = getenv(xdg.environmental);
    const gchar *home         = getenv("HOME");

    if (!actual_value || !actual_value[0])
        actual_value = xdg.default_value;

    if (!actual_value)
        return NULL;

    return str_replace("~", home, actual_value);
}

/*@null@*/ gchar*
find_xdg_file (int xdg_type, const char* basename) {
    /* xdg_type = 0 => config
       xdg_type = 1 => data
       xdg_type = 2 => cache  */

    gchar *xdgv = get_xdg_var(XDG[xdg_type]);
    gchar *path = g_strconcat (xdgv, basename, NULL);
    g_free (xdgv);

    if (file_exists(path))
        return path;

    if (xdg_type == 2)
        return NULL;

    /* the file doesn't exist in the expected directory.
     * check if it exists in one of the system-wide directories. */
    char *system_dirs = get_xdg_var(XDG[3 + xdg_type]);
    path = find_existing_file2(system_dirs, basename);
    g_free(system_dirs);

    return path;
}

gboolean
file_exists (const char * filename) {
    return (access(filename, F_OK) == 0);
}

char *
str_replace (const char* search, const char* replace, const char* string) {
    gchar **buf;
    char *ret;

    if(!string)
        return NULL;

    buf = g_strsplit (string, search, -1);
    ret = g_strjoinv (replace, buf);
    g_strfreev(buf);

    return ret;
}

gboolean
for_each_line_in_file(const gchar *path, void (*callback)(const gchar *l, void *c), void *user_data) {
    gchar *line = NULL;
    gsize len;

    GIOChannel *chan = g_io_channel_new_file(path, "r", NULL);

    if (!chan)
        return FALSE;

    while (g_io_channel_read_line(chan, &line, &len, NULL, NULL) == G_IO_STATUS_NORMAL) {
        callback(line, user_data);
        g_free(line);
    }

    g_io_channel_unref (chan);

    return TRUE;
}

/* This function searches the directories in the : separated ($PATH style)
 * string 'dirs' for a file named 'basename'. It returns the path of the first
 * file found, or NULL if none could be found.
 * NOTE: this function modifies the 'dirs' argument. */
gchar*
find_existing_file2(gchar *dirs, const gchar *basename) {
    char *saveptr = NULL;

    /* iterate through the : separated elements until we find our file. */
    char *tok  = strtok_r(dirs, ":", &saveptr);
    char *path = g_strconcat (tok, "/", basename, NULL);

    while (!file_exists(path)) {
        g_free(path);

        tok = strtok_r(NULL, ":", &saveptr);
        if (!tok)
            return NULL; /* we've hit the end of the string */

        path = g_strconcat (tok, "/", basename, NULL);
    }

    return path;
}

/* search a PATH style string for an existing file+path combination.
 * everything after the last ':' is assumed to be the name of the file.
 * e.g. "/tmp:/home:a/file" will look for /tmp/a/file and /home/a/file.
 *
 * if there are no :s then the entire thing is taken to be the path. */
gchar*
find_existing_file(const gchar* path_list) {
    if(!path_list)
        return NULL;

    char *path_list_dup = g_strdup(path_list);

    char *basename = strrchr(path_list_dup, ':');
    if(!basename)
      return file_exists(path_list_dup) ? path_list_dup : NULL;

    basename[0] = '\0';
    basename++;

    char *result = find_existing_file2(path_list_dup, basename);
    g_free(path_list_dup);
    return result;
}

gchar*
argv_idx(const GArray *a, const guint idx) {
    return g_array_index(a, gchar*, idx);
}

GString *
append_escaped (GString *dest, const gchar *src) {
    g_assert(dest);
    g_assert(src);

    // Hint that we are going to append another string.
    int oldlen = dest->len;
    g_string_set_size (dest, dest->len + strlen(src) * 2);
    g_string_truncate (dest, oldlen);

    // Append src char by char with baddies escaped
    for (const gchar *p = src; *p; p++) {
        switch (*p) {
        case '\\':
            g_string_append (dest, "\\\\");
            break;
        case '\'':
            g_string_append (dest, "\\'");
            break;
        case '\n':
            g_string_append (dest, "\\n");
            break;
        default:
            g_string_append_c (dest, *p);
            break;
        }
    }
    return dest;
}
