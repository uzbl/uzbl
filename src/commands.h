#ifndef UZBL_COMMANDS_H
#define UZBL_COMMANDS_H

#include <glib.h>
#include <gio/gio.h>

struct _UzblCommand;
typedef struct _UzblCommand UzblCommand;

GArray *
uzbl_commands_args_new ();
void
uzbl_commands_args_append (GArray *argv, const gchar *arg);
void
uzbl_commands_args_free (GArray *argv);

const UzblCommand *
uzbl_commands_parse (const gchar *cmd, GArray *argv);
void
uzbl_commands_parse_async (const gchar         *cmd,
                           GArray              *argv,
                           GAsyncReadyCallback  callback,
                           gpointer             data);
const UzblCommand *
uzbl_command_parse_finish (GObject       *source,
                           GAsyncResult  *res,
                           GError       **error);
void
uzbl_commands_run_parsed (const UzblCommand *info, GArray *argv, GString *result);
void
uzbl_commands_run_async (const UzblCommand   *info,
                         GArray              *argv,
                         gboolean             capture,
                         GAsyncReadyCallback  callback,
                         gpointer             data);
GString*
uzbl_commands_run_finish (GObject       *source,
                          GAsyncResult  *res,
                          GError       **error);
void
uzbl_commands_run_argv (const gchar *cmd, GArray *argv, GString *result);
void
uzbl_commands_run (const gchar *cmd, GString *result);

void
uzbl_commands_load_file (const gchar *path);

#endif
