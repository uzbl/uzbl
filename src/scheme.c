#include "scheme.h"

#ifndef USE_WEBKIT2
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
