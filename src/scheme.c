#include "scheme.h"

#ifdef USE_WEBKIT2
#include "commands.h"
#include "io.h"
#else
#include "scheme-request.h"
#endif
#include "uzbl-core.h"

/* =========================== PUBLIC API =========================== */

void
uzbl_scheme_init ()
{
#ifndef USE_WEBKIT2
    soup_session_add_feature_by_type (uzbl.net.soup_session, UZBL_TYPE_SCHEME_REQUEST);
#endif
}

#ifdef USE_WEBKIT2
static void
scheme_callback (WebKitURISchemeRequest *request, gpointer data);
#endif

void
uzbl_scheme_add_handler (const gchar *scheme, const gchar *command)
{
#ifdef USE_WEBKIT2
    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    webkit_web_context_register_uri_scheme (context, scheme, scheme_callback, g_strdup (command), g_free);
#else
    uzbl_scheme_request_add_handler (scheme, command);
#endif
}

#ifdef USE_WEBKIT2
static void
scheme_return (GString *result, gpointer data);

void
scheme_callback (WebKitURISchemeRequest *request, gpointer data)
{
    gchar *command = (gchar *)data;
    const gchar *uri = webkit_uri_scheme_request_get_uri (request);

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse (command, args);

    if (!cmd) {
        uzbl_commands_args_free (args);
        return;
    }

    uzbl_commands_args_append (args, g_strdup (uri));

    uzbl_io_schedule_command (cmd, args, scheme_return, request);
}

void
scheme_return (GString *result, gpointer data)
{
    WebKitURISchemeRequest *request = (WebKitURISchemeRequest *)data;

    gint64 len = result->len;
    GInputStream *stream = g_memory_input_stream_new_from_data (
        g_strdup (result->str),
        len, g_free);

    webkit_uri_scheme_request_finish (request, stream, len, "text/html");

    g_object_unref (stream);
}
#endif
