/*
 * Uzbl Commands
 */
#ifndef UZBL_COMMANDS_H
#define UZBL_COMMANDS_H

#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

typedef void (*Command)(WebKitWebView*, GArray *argv, GString *result);

typedef struct {
    const gchar *name;
    Command      function;
    gboolean     split;
} CommandInfo;

/**
 * Initialises the hash table uzbl.behave.commands with the available commands.
 */
void
commands_hash();

/**
 * Sends the BUILTINS events with the available commands.
 */
void
builtins();

void        run_external_js (WebKitWebView * web_view, GArray *argv, GString *result);

gchar**     split_quoted(const gchar* src, const gboolean unquote);
void        parse_cmd_line(const char *ctl_line, GString *result);
const CommandInfo *
            parse_command_parts(const gchar *line, GArray *a);
void        parse_command_arguments(const gchar *p, GArray *a, gboolean no_split);
void        run_parsed_command(const CommandInfo *c, GArray *a, GString *result);

#endif
