#ifndef UZBL_IO_H
#define UZBL_IO_H

#include <glib.h>

void
uzbl_io_init ();
void
uzbl_io_init_stdin ();
void
uzbl_io_init_connect_socket ();

gboolean
uzbl_io_init_fifo (const gchar *dir);
gboolean
uzbl_io_init_socket (const gchar *dir);

#endif
