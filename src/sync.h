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

#define _uzbl_sync_call(res, obj, err, send, ...)                                        \
    _uzbl_sync_call_args (res, send, (obj, ## __VA_ARGS__, NULL, uzbl_sync_close, data), \
                          send##_finish, (obj, data->result, &err))
#define _uzbl_sync_call_args(res, send, send_args,  \
                             finish, finish_args)   \
    do                                              \
    {                                               \
        UzblSyncData *data = uzbl_sync_data_new (); \
                                                    \
        send send_args;                             \
        g_main_loop_run (data->loop);               \
        res finish finish_args;                     \
                                                    \
        uzbl_sync_data_free (data);                 \
    } while (0)

#define uzbl_sync_call_void(obj, err, send, ...) \
    _uzbl_sync_call(, obj, err, send, ## __VA_ARGS__)

#define uzbl_sync_call(res, obj, err, send, ...) \
    _uzbl_sync_call(res =, obj, err, send, ## __VA_ARGS__)

#endif
