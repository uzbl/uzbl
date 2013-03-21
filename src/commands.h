/*
 * Uzbl Commands
 */

#ifndef UZBL_COMMANDS_H
#define UZBL_COMMANDS_H

#include "uzbl-core.h"

typedef struct _UzblCommandInfo UzblCommandInfo;

void
uzbl_commands_init ();

void
uzbl_commands_send_builtin_event ();

void /* FIXME: This should not be public here. */
eval_js (WebKitWebView *view, const gchar *script, GString *result, const gchar *path);
void /* FIXME: This should not be public here. */
cmd_js (WebKitWebView *view, GArray *argv, GString *result);
void /* FIXME: This should not be public here. */
cmd_js_file (WebKitWebView *view, GArray *argv, GString *result);

gchar**     split_quoted(const gchar* src, const gboolean unquote);
void        parse_cmd_line(const char *ctl_line, GString *result);
const UzblCommandInfo *
            parse_command_parts(const gchar *line, GArray *a);
void        parse_command_arguments(const gchar *p, GArray *a, gboolean no_split);
void        run_parsed_command(const UzblCommandInfo *c, GArray *a, GString *result);

#endif
