/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2007  Jonathan Matthew
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  The Rhythmbox authors hereby grant permission for non-GPL compatible
 *  GStreamer plugins to be used and distributed together with GStreamer
 *  and Rhythmbox. This permission is above and beyond the permissions granted
 *  by the GPL license by which Rhythmbox is covered. If you modify this code
 *  you may extend this exception to your version of the code, but you are not
 *  obligated to do so. If you do not wish to do so, delete this exception
 *  statement from your version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */

/*#include "config.h"*/

#include "rb-async-queue-watch.h"

/**
 * SECTION:rb-async-queue-watch
 * @short_description: GSource for watching a GAsyncQueue in the main loop
 *
 * This provides a way to feed work items to the main loop using a #GAsyncQueue
 * without polling it.
 */

/**
 * RBAsyncQueueWatchFunc:
 * @item: the item found in the queue
 * @data: user data specified when creating the watch
 *
 * Callback to call when an item is found in the queue.
 */

typedef struct {
	GSource source;
	GAsyncQueue *queue;
} UzblRBAsyncQueueWatch;

static gboolean
uzbl_rb_async_queue_watch_prepare (GSource *source, gint *timeout)
{
	UzblRBAsyncQueueWatch *watch = (UzblRBAsyncQueueWatch *)source;
	*timeout = -1;
	return (g_async_queue_length (watch->queue) > 0);
}

static gboolean
uzbl_rb_async_queue_watch_check (GSource *source)
{
	UzblRBAsyncQueueWatch *watch = (UzblRBAsyncQueueWatch *)source;
	return (g_async_queue_length (watch->queue) > 0);
}

static gboolean
uzbl_rb_async_queue_watch_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	UzblRBAsyncQueueWatch *watch = (UzblRBAsyncQueueWatch *)source;
	UzblRBAsyncQueueWatchFunc cb = (UzblRBAsyncQueueWatchFunc)callback;
	gpointer item;

	item = g_async_queue_try_pop (watch->queue);
	if (item == NULL) {
		return TRUE;
	}

	if (cb == NULL) {
		return FALSE;
	}

	cb (item, user_data);
	return TRUE;
}

static void
uzbl_rb_async_queue_watch_finalize (GSource *source)
{
	UzblRBAsyncQueueWatch *watch = (UzblRBAsyncQueueWatch *)source;

	if (watch->queue != NULL) {
		g_async_queue_unref (watch->queue);
		watch->queue = NULL;
	}
}

static GSourceFuncs uzbl_rb_async_queue_watch_funcs = {
	.prepare  = uzbl_rb_async_queue_watch_prepare,
	.check    = uzbl_rb_async_queue_watch_check,
	.dispatch = uzbl_rb_async_queue_watch_dispatch,
	.finalize = uzbl_rb_async_queue_watch_finalize
};

/**
 * rb_async_queue_watch_new:
 * @queue:	the #GAsyncQueue to watch
 * @priority:	priority value for the #GSource
 * @callback:	callback to invoke when the queue is non-empty
 * @user_data:	user data to pass to the callback
 * @notify:	function to call to clean up the user data for the callback
 * @context:	the #GMainContext to attach the source to
 *
 * Creates a new #GSource that triggers when the #GAsyncQueue is
 * non-empty.  This is used in rhythmbox to process queues within
 * #RhythmDB in the main thread without polling.
 *
 * Return value: the ID of the new #GSource
 */
guint uzbl_rb_async_queue_watch_new (GAsyncQueue *queue,
				gint priority,
				UzblRBAsyncQueueWatchFunc callback,
				gpointer user_data,
				GDestroyNotify notify,
				GMainContext *context)
{
	GSource *source;
	UzblRBAsyncQueueWatch *watch;
	guint id;

	source = (GSource *) g_source_new (&uzbl_rb_async_queue_watch_funcs,
					   sizeof (UzblRBAsyncQueueWatch));

	watch = (UzblRBAsyncQueueWatch *)source;
	watch->queue = g_async_queue_ref (queue);

	if (priority != G_PRIORITY_DEFAULT)
		g_source_set_priority (source, priority);

	g_source_set_callback (source, (GSourceFunc) callback, user_data, notify);

	id = g_source_attach (source, context);
	g_source_unref (source);
	return id;
}
