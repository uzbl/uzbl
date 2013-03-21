#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "util.h"

gchar* find_existing_file2(gchar *, const gchar *);

typedef struct {
    const gchar* environmental;
    const gchar* default_value;
} XDG_Var;

const XDG_Var XDG[] = {
    { "XDG_CONFIG_HOME", "~/.config" },
    { "XDG_DATA_HOME",   "~/.local/share" },
    { "XDG_CACHE_HOME",  "~/.cache" },
    { "XDG_CONFIG_DIRS", "/etc/xdg" },
    { "XDG_DATA_DIRS",   "/usr/local/share/:/usr/share/" },
};

/*@null@*/ gchar*
get_xdg_var (enum xdg_type type) {
    XDG_Var xdg = XDG[type];

    const gchar *actual_value = getenv(xdg.environmental);
    const gchar *home         = getenv("HOME");

    if (!actual_value || !actual_value[0])
        actual_value = xdg.default_value;

    if (!actual_value)
        return NULL;

    return str_replace("~", home, actual_value);
}

void
ensure_xdg_vars (void) {
    int i;

    for (i = 0; i <= 2; ++i) {
        gchar* xdg = get_xdg_var(i);

        if (!xdg)
            continue;

        setenv(XDG[i].environmental, xdg, 0);

        g_free(xdg);
    }
}

/*@null@*/ gchar*
find_xdg_file (enum xdg_type type, const char* basename) {
    gchar *dirs = get_xdg_var(type);
    gchar *path = g_strconcat (dirs, basename, NULL);
    g_free (dirs);

    if (file_exists(path))
        return path; /* we found the file */

    if (type == XDG_CACHE)
        return NULL; /* there's no system cache directory */

    /* the file doesn't exist in the expected directory.
     * check if it exists in one of the system-wide directories. */
    char *system_dirs = get_xdg_var(3 + type);
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
