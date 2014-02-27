#include <glib.h>

#include "util.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

/* =========================== PUBLIC API =========================== */

gchar *
argv_idx (const GArray *argv, guint idx)
{
    return g_array_index (argv, gchar *, idx);
}

gchar *
str_replace (const gchar *needle, const gchar *replace, const gchar *haystack)
{
    gchar **buf;
    char *ret;

    if (!haystack) {
        return NULL;
    }

    buf = g_strsplit (haystack, needle, -1);
    ret = g_strjoinv (replace, buf);
    g_strfreev(buf);

    return ret;
}

void
remove_trailing_newline (const char *line)
{
    char *p = strchr (line, '\n');
    if (p != NULL) {
        *p = '\0';
    }
}

gboolean
file_exists (const char *filename)
{
    struct stat st;
    return !lstat (filename, &st);
}

gchar *
find_existing_file (const gchar *path_list)
{
    if (!path_list) {
        return NULL;
    }

    char *path_list_dup = g_strdup (path_list);

    char *basename = strrchr (path_list_dup, ':');
    if (!basename) {
        if (file_exists (path_list_dup)) {
            return path_list_dup;
        }

        g_free (path_list_dup);

        return NULL;
    }

    *basename = '\0';
    ++basename;

    char *result = find_existing_file_options (path_list_dup, basename);
    g_free (path_list_dup);

    return result;
}

gchar *
find_existing_file_options (gchar *dirs, const gchar *basename)
{
    char *saveptr = NULL;

    /* Iterate through the ':' separated elements until we find our file. */
    char *tok  = strtok_r (dirs, ":", &saveptr);
    char *path = g_strconcat (tok, "/", basename, NULL);

    while (!file_exists (path)) {
        g_free (path);

        tok = strtok_r (NULL, ":", &saveptr);
        if (!tok) {
            return NULL; /* We've hit the end of the string. */
        }

        path = g_strconcat (tok, "/", basename, NULL);
    }

    return path;
}
