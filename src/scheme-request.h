#ifndef UZBL_SCHEME_REQUEST_H
#define UZBL_SCHEME_REQUEST_H

#include <libsoup/soup-version.h>

#if !SOUP_CHECK_VERSION (2, 41, 3)
#define LIBSOUP_USE_UNSTABLE_REQUEST_API
#endif

#include <libsoup/soup-request.h>

#define UZBL_TYPE_SCHEME_REQUEST            (uzbl_scheme_request_get_type ())
#define UZBL_SCHEME_REQUEST(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), UZBL_TYPE_SCHEME_REQUEST, UzblSchemeRequest))
#define UZBL_SCHEME_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UZBL_TYPE_SCHEME_REQUEST, UzblSchemeRequestClass))
#define UZBL_IS_SCHEME_REQUEST(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), UZBL_TYPE_SCHEME_REQUEST))
#define UZBL_IS_SCHEME_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UZBL_TYPE_SCHEME_REQUEST))
#define UZBL_SCHEME_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UZBL_TYPE_SCHEME_REQUEST, UzblSchemeRequestClass))

typedef struct _UzblSchemeRequestPrivate UzblSchemeRequestPrivate;

typedef struct {
    SoupRequest parent;

    UzblSchemeRequestPrivate *priv;
} UzblSchemeRequest;

typedef struct {
    SoupRequestClass parent;
    GArray *schemes;
    GHashTable *handlers;
} UzblSchemeRequestClass;

GType
uzbl_scheme_request_get_type ();

void
uzbl_scheme_request_add_handler (const gchar *scheme, const gchar *command);

#endif
