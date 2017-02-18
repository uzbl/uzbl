#ifndef UZBL_IO_H
#define UZBL_IO_H

#include "commands.h"
#include "extio.h"

#include <glib.h>

void
uzbl_io_send (const gchar *message, gboolean connect_only);

void
uzbl_io_schedule_command (const UzblCommand   *cmd,
                          GArray              *argv,
                          GAsyncReadyCallback  callback,
                          gpointer             data);
GString *
uzbl_io_command_finish (GObject       *source,
                        GAsyncResult  *result,
                        GError       **error);

void
uzbl_io_send_ext_message (ExtIOMessageType type, ...);

gboolean
uzbl_io_init_fifo (const gchar *dir);
gboolean
uzbl_io_init_socket (const gchar *dir);
gboolean
uzbl_io_init_extpipe ();
void
uzbl_io_extfds(int *input, int *output);


// The input processing function should create a new GTask for processing the
// line of input that will call the given callback when done. It may resolve
// the task immediately.
typedef void (*UzblProcessLineFunc) (const gchar         *input,
                                     gpointer             data,
                                     GAsyncReadyCallback  callback,
                                     gpointer             callback_data);

void
uzbl_io_process_lines_async (GInputStream        *stream,
                             UzblProcessLineFunc  processor,
                             GAsyncReadyCallback  callback,
                             gpointer             data);

void
uzbl_io_process_lines_finish (GObject       *source,
                              GAsyncResult  *result,
                              GError       **error);

#endif
