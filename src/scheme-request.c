#include "scheme-request.h"

#include "util.h"

/* =========================== PUBLIC API =========================== */

static void
uzbl_scheme_request_init (UzblSchemeRequest *request);
static void
uzbl_scheme_request_class_init (UzblSchemeRequestClass *uzbl_scheme_request_class);

G_DEFINE_TYPE (UzblSchemeRequest, uzbl_scheme_request, SOUP_TYPE_REQUEST)

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static void
uzbl_scheme_request_finalize (GObject *obj);

static const char *
uzbl_schemes[] = {
    "uzbl",
    NULL
};

static gboolean
uzbl_scheme_request_check_uri (SoupRequest *request, SoupURI *uri, GError **error);
static GInputStream *
uzbl_scheme_request_send (SoupRequest *request, GCancellable *cancellable, GError **error);
static goffset
uzbl_scheme_request_get_content_length (SoupRequest *request);
static const char *
uzbl_scheme_request_get_content_type (SoupRequest *request);

void
uzbl_scheme_request_init (UzblSchemeRequest *request)
{
    UZBL_UNUSED (request);
}

void
uzbl_scheme_request_class_init (UzblSchemeRequestClass *uzbl_scheme_request_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (uzbl_scheme_request_class);
    SoupRequestClass *scheme_request_class = SOUP_REQUEST_CLASS (uzbl_scheme_request_class);

    gobject_class->finalize = uzbl_scheme_request_finalize;

    scheme_request_class->schemes = uzbl_schemes;
    scheme_request_class->check_uri = uzbl_scheme_request_check_uri;
    scheme_request_class->send = uzbl_scheme_request_send;
    scheme_request_class->get_content_length = uzbl_scheme_request_get_content_length;
    scheme_request_class->get_content_type = uzbl_scheme_request_get_content_type;
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
    UZBL_UNUSED (request);
    UZBL_UNUSED (cancellable);
    UZBL_UNUSED (error);

    return g_memory_input_stream_new_from_data (g_strdup ("UZBL!"), 6, g_free);
}

goffset
uzbl_scheme_request_get_content_length (SoupRequest *request)
{
    UZBL_UNUSED (request);

    return 6;
}

const char *
uzbl_scheme_request_get_content_type (SoupRequest *request)
{
    UZBL_UNUSED (request);

    return "text/html";
}
