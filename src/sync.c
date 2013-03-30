#include "sync.h"

#include "util.h"

/* =========================== PUBLIC API =========================== */

UzblSyncData *
uzbl_sync_data_new ()
{
    UzblSyncData *data = (UzblSyncData *)g_malloc (sizeof (UzblSyncData));

    data->context = g_main_context_new ();
    g_main_context_push_thread_default (data->context);
    data->loop = g_main_loop_new (data->context, TRUE);
    data->result = NULL;

    return data;
}

void
uzbl_sync_data_free (UzblSyncData *data)
{
    g_main_loop_unref (data->loop);
    g_main_context_pop_thread_default (data->context);
    g_main_context_unref (data->context);

    g_free (data);
}

void
uzbl_sync_close (GObject *object, GAsyncResult *res, gpointer data)
{
    UZBL_UNUSED (object);

    UzblSyncData* sync_data = (UzblSyncData *)data;

    sync_data->result = g_object_ref (res);
    g_main_loop_quit (sync_data->loop);
}
