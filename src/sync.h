#ifndef UZBL_SYNC_H
#define UZBL_SYNC_H

#include <glib.h>
#include <gio/gio.h>

typedef struct {
    GMainContext *context;
    GMainLoop    *loop;
    GAsyncResult *result;
} UzblSyncData;

UzblSyncData *
uzbl_sync_data_new ();
void
uzbl_sync_data_free (UzblSyncData *data);

void
uzbl_sync_close (GObject *object, GAsyncResult *result, gpointer data);

#define uzbl_sync_call_void(data, obj, err, send, ...)                                  \
    uzbl_sync_call_args_void (data,                                                     \
                              send, (obj, ## __VA_ARGS__, NULL, uzbl_sync_close, data), \
                              send##_finish, (obj, data->result, &err))
#define uzbl_sync_call_args_void(data,                \
                                 send, send_args,     \
                                 finish, finish_args) \
    do                                                \
    {                                                 \
        send send_args;                               \
        g_main_loop_run (data->loop);                 \
        finish finish_args;                           \
    } while (0)

#define uzbl_sync_call(data, res, obj, err, send, ...)                             \
    uzbl_sync_call_args (data, res,                                                \
                         send, (obj, ## __VA_ARGS__, NULL, uzbl_sync_close, data), \
                         send##_finish, (obj, data->result, &err))
#define uzbl_sync_call_args(data, result,        \
                            send, send_args,     \
                            finish, finish_args) \
    do                                           \
    {                                            \
        send send_args;                          \
        g_main_loop_run (data->loop);            \
        result = finish finish_args;             \
    } while (0)

#endif
