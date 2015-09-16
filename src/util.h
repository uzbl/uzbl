/* Header guard intentionally not added. This file is only meant for .c files. */

#define UZBL_UNUSED(var) (void)var

#define ARG_CHECK(argv, count)   \
    do                           \
    {                            \
        if (argv->len < count) { \
            return;              \
        }                        \
    } while (false)

gchar *
argv_idx (const GArray *argv, guint idx);

gchar *
str_replace (const gchar *needle, const gchar *replace, const gchar *haystack);

void
remove_trailing_newline (const char *line);

gboolean
file_exists (const char *filename);
/* Search a PATH style string for an existing file+path combination. everything
 * after the last ':' is assumed to be the name of the file. e.g.
 * "/tmp:/home:a/file" will look for /tmp/a/file and /home/a/file.
 *
 * If there are no ':'s, the entire thing is taken to be the path. */
gchar *
find_existing_file (const gchar *path_list);
/* This function searches the directories in the ':' separated ($PATH style)
 * string 'dirs' for a file named 'basename'. It returns the path of the first
 * file found, or NULL if none could be found.
 *
 * NOTE: this function modifies the 'dirs' argument. */
gchar *
find_existing_file_options (gchar *dirs, const gchar *basename);
