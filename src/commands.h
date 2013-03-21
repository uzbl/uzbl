#ifndef UZBL_COMMANDS_H
#define UZBL_COMMANDS_H

#include "webkit.h" /* FIXME */

#include <glib.h>

struct _UzblCommand;
typedef struct _UzblCommand UzblCommand;

void
uzbl_commands_init ();

void
uzbl_commands_send_builtin_event ();

const UzblCommand *
uzbl_commands_parse (const gchar *cmd, GArray *argv);
void
uzbl_commands_run_parsed (const UzblCommand *info, GArray *argv, GString *result);
void
uzbl_commands_run_argv (const gchar *cmd, GArray *argv, GString *result);
void
uzbl_commands_run (const gchar *cmd, GString *result);

void
uzbl_commands_load_file (const gchar *path);

#endif
