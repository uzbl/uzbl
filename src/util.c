#define _POSIX_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "util.h"

const XDG_Var XDG[] = {
    { "XDG_CONFIG_HOME", "~/.config" },
    { "XDG_DATA_HOME",   "~/.local/share" },
    { "XDG_CACHE_HOME",  "~/.cache" },
    { "XDG_CONFIG_DIRS", "/etc/xdg" },
    { "XDG_DATA_DIRS",   "/usr/local/share/:/usr/share/" },
};

/*@null@*/ gchar*
get_xdg_var (XDG_Var xdg) {
    const gchar* actual_value = getenv (xdg.environmental);
    const gchar* home         = getenv ("HOME");
    gchar* return_value;

    if (! actual_value || strcmp (actual_value, "") == 0) {
        if (xdg.default_value) {
            return_value = str_replace ("~", home, xdg.default_value);
        } else {
            return_value = NULL;
        }
    } else {
        return_value = str_replace("~", home, actual_value);
    }

    return return_value;
}

/*@null@*/ gchar*
find_xdg_file (int xdg_type, const char* filename) {
    /* xdg_type = 0 => config
       xdg_type = 1 => data
       xdg_type = 2 => cache*/

    gchar* xdgv = get_xdg_var (XDG[xdg_type]);
    gchar* temporary_file = g_strconcat (xdgv, filename, NULL);
    g_free (xdgv);

    gchar* temporary_string;
    char*  saveptr;
    char*  buf;

    if (! file_exists (temporary_file) && xdg_type != 2) {
        buf = get_xdg_var (XDG[3 + xdg_type]);
        temporary_string = (char *) strtok_r (buf, ":", &saveptr);
        g_free(buf);

        while ((temporary_string = (char * ) strtok_r (NULL, ":", &saveptr)) && ! file_exists (temporary_file)) {
            g_free (temporary_file);
            temporary_file = g_strconcat (temporary_string, filename, NULL);
        }
    }

    //g_free (temporary_string); - segfaults.

    if (file_exists (temporary_file)) {
        return temporary_file;
    } else {
        g_free(temporary_file);
        return NULL;
    }
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

    if (chan) {
        while (g_io_channel_read_line(chan, &line, &len, NULL, NULL) == G_IO_STATUS_NORMAL) {
          callback(line, user_data);
          g_free(line);
        }
        g_io_channel_unref (chan);

        return TRUE;
    }

    return FALSE;
}


enum exp_type
get_exp_type(const gchar *s) {
    /* variables */
    if(*(s+1) == '(')
        return EXP_EXPR;
    else if(*(s+1) == '{')
        return EXP_BRACED_VAR;
    else if(*(s+1) == '<')
        return EXP_JS;
    else if(*(s+1) == '[')
        return EXP_ESCAPE;
    else
        return EXP_SIMPLE_VAR;

    /*@notreached@*/
return EXP_ERR;
}


/* search a PATH style string for an existing file+path combination */
gchar*
find_existing_file(gchar* path_list) {
    int i=0;
    int cnt;
    gchar **split;
    gchar *tmp = NULL;
    gchar *executable;

    if(!path_list)
        return NULL;

    split = g_strsplit(path_list, ":", 0);
    while(split[i])
        i++;

    if(i<=1) {
        tmp = g_strdup(split[0]);
        g_strfreev(split);
        return tmp;
    }
    else
        cnt = i-1;

    i=0;
    tmp = g_strdup(split[cnt]);
    g_strstrip(tmp);
    if(tmp[0] == '/')
        executable = g_strdup_printf("%s", tmp+1);
    else
        executable = g_strdup(tmp);
    g_free(tmp);

    while(i<cnt) {
        tmp = g_strconcat(g_strstrip(split[i]), "/", executable, NULL);
        if(g_file_test(tmp, G_FILE_TEST_EXISTS)) {
            g_strfreev(split);
            return tmp;
        }
        else
            g_free(tmp);
        i++;
    }

    g_free(executable);
    g_strfreev(split);
    return NULL;
}


char*
itos(int val) {
    char tmp[20];

    snprintf(tmp, sizeof(tmp), "%i", val);
    return g_strdup(tmp);
}

gchar*
argv_idx(const GArray *a, const guint idx) {
    return g_array_index(a, gchar*, idx);
}
