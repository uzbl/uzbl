#ifndef UZBL_IO_H
#define UZBL_IO_H

#include "commands.h"

#include <glib.h>

void
uzbl_io_init ();
void
uzbl_io_free ();
void
uzbl_io_init_stdin ();
gboolean
uzbl_io_init_connect_socket (const gchar *socket_path);

void
uzbl_io_send (const gchar *message, gboolean connect_only);

typedef void (*UzblIOCallback)(GString *result, gpointer data);

void
uzbl_io_schedule_command (const UzblCommand *cmd, GArray *argv, UzblIOCallback callback, gpointer data);

gboolean
uzbl_io_init_fifo (const gchar *dir);
gboolean
uzbl_io_init_socket (const gchar *dir);

#endif
