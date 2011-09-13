#ifndef __IO__
#define __IO__

#include <glib/gstdio.h>

/*@null@*/ gchar*
build_stream_name(int type, const gchar *dir);

gboolean    control_fifo(GIOChannel *gio, GIOCondition condition);

gboolean    init_fifo(const gchar *dir);

gboolean    control_stdin(GIOChannel *gio, GIOCondition condition);
void        create_stdin();

gboolean    init_socket(const gchar *dir);

gboolean    control_socket(GIOChannel *chan);
gboolean    control_client_socket(GIOChannel *chan);

#endif
