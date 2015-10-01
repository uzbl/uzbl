#include "scheme-request.h"

#include "commands.h"
#include "util.h"

#include <libsoup/soup-uri.h>

#include <string.h>

/* =========================== PUBLIC API =========================== */

static void
uzbl_scheme_request_init (UzblSchemeRequest *request);
static void
uzbl_scheme_request_class_init (UzblSchemeRequestClass *uzbl_scheme_request_class);

G_DEFINE_TYPE (UzblSchemeRequest, uzbl_scheme_request, SOUP_TYPE_REQUEST)

void
uzbl_scheme_request_add_handler (const gchar *scheme, const gchar *command)
{
    UzblSchemeRequestClass *uzbl_scheme_request_class = g_type_class_ref (UZBL_TYPE_SCHEME_REQUEST);
    SoupRequestClass *request_class = SOUP_REQUEST_CLASS (uzbl_scheme_request_class);

    char *scheme_dup = g_strdup (scheme);
    g_hash_table_insert (uzbl_scheme_request_class->handlers,
        scheme_dup, g_strdup (command));
    g_array_append_val (uzbl_scheme_request_class->schemes, scheme_dup);
    request_class->schemes = (const char **)uzbl_scheme_request_class->schemes->data;
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static void
uzbl_scheme_request_finalize (GObject *obj);

static gboolean
uzbl_scheme_request_check_uri (SoupRequest *request, SoupURI *uri, GError **error);
static GInputStream *
uzbl_scheme_request_send (SoupRequest *request, GCancellable *cancellable, GError **error);
static goffset
uzbl_scheme_request_get_content_length (SoupRequest *request);
static const char *
uzbl_scheme_request_get_content_type (SoupRequest *request);

struct _UzblSchemeRequestPrivate
{
    gssize content_length;
    gchar *content_type;
};

void
uzbl_scheme_request_init (UzblSchemeRequest *request)
{
    request->priv = G_TYPE_INSTANCE_GET_PRIVATE (request, UZBL_TYPE_SCHEME_REQUEST, UzblSchemeRequestPrivate);
    request->priv->content_length = 0;
    request->priv->content_type = NULL;
}

void
uzbl_scheme_request_class_init (UzblSchemeRequestClass *uzbl_scheme_request_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (uzbl_scheme_request_class);
    SoupRequestClass *scheme_request_class = SOUP_REQUEST_CLASS (uzbl_scheme_request_class);

    uzbl_scheme_request_class->schemes = g_array_new (TRUE, TRUE, sizeof (gchar *));
    uzbl_scheme_request_class->handlers = g_hash_table_new (g_str_hash, g_str_equal);

    gobject_class->finalize = uzbl_scheme_request_finalize;

    scheme_request_class->schemes = (const char **)uzbl_scheme_request_class->schemes->data;
    scheme_request_class->check_uri = uzbl_scheme_request_check_uri;
    scheme_request_class->send = uzbl_scheme_request_send;
    scheme_request_class->get_content_length = uzbl_scheme_request_get_content_length;
    scheme_request_class->get_content_type = uzbl_scheme_request_get_content_type;

    g_type_class_add_private (uzbl_scheme_request_class, sizeof (UzblSchemeRequestPrivate));
}

void
uzbl_scheme_request_finalize (GObject *obj)
{
    G_OBJECT_CLASS (uzbl_scheme_request_parent_class)->finalize (obj);
}

gboolean
uzbl_scheme_request_check_uri (SoupRequest *request, SoupURI *uri, GError **error)
{
    UZBL_UNUSED (request);
    UZBL_UNUSED (uri);
    UZBL_UNUSED (error);

    return TRUE;
}

GInputStream *
uzbl_scheme_request_send (SoupRequest *request, GCancellable *cancellable, GError **error)
{
    UZBL_UNUSED (cancellable);
    UZBL_UNUSED (error);

    UzblSchemeRequest *uzbl_request = UZBL_SCHEME_REQUEST (request);
    UzblSchemeRequestClass *cls = UZBL_SCHEME_REQUEST_GET_CLASS (uzbl_request);

    SoupURI *uri = soup_request_get_uri (request);
    const char *command = g_hash_table_lookup (cls->handlers, uri->scheme);

    GString *result = g_string_new ("");
    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *cmd = uzbl_commands_parse (command, args);

    if (cmd) {
        uzbl_commands_args_append (args, soup_uri_to_string (uri, TRUE));
        uzbl_commands_run_parsed (cmd, args, result);
    }

    uzbl_commands_args_free (args);

    gchar *end = strchr (result->str, '\n');
    size_t line_len = end ? (size_t)(end - result->str) : result->len;

    uzbl_request->priv->content_length = result->len - line_len - 1;
    uzbl_request->priv->content_type = g_strndup (result->str, line_len);
    GInputStream *stream = g_memory_input_stream_new_from_data (
        g_strdup (end + 1),
        uzbl_request->priv->content_length, g_free);
    g_string_free (result, TRUE);

    return stream;
}

goffset
uzbl_scheme_request_get_content_length (SoupRequest *request)
{
    UzblSchemeRequest *uzbl_request = UZBL_SCHEME_REQUEST (request);

    return uzbl_request->priv->content_length;
}

const char *
uzbl_scheme_request_get_content_type (SoupRequest *request)
{
    UzblSchemeRequest *uzbl_request = UZBL_SCHEME_REQUEST (request);

    return uzbl_request->priv->content_type ? uzbl_request->priv->content_type : "text/html";
}
