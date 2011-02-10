#ifndef __IO__
#define __IO__

#include <glib/gstdio.h>

/*@null@*/ gchar*
build_stream_name(int type, const gchar *dir);

gboolean    control_fifo(GIOChannel *gio, GIOCondition condition);

/*@null@*/ gchar*
init_fifo(gchar *dir);

gboolean    control_stdin(GIOChannel *gio, GIOCondition condition);
void        create_stdin();

/*@null@*/ gchar*
init_socket(gchar *dir);

gboolean    control_socket(GIOChannel *chan);
gboolean    control_client_socket(GIOChannel *chan);

#endif
