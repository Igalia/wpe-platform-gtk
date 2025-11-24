/*
 * Copyright (c) 2025 Igalia S.L.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "wpe-clipboard-gtk.h"

struct _WPEClipboardGtk {
  WPEClipboard parent;

  GdkClipboard *clipboard;
};

G_DEFINE_FINAL_TYPE(WPEClipboardGtk, wpe_clipboard_gtk, WPE_TYPE_CLIPBOARD)

static void wpe_clipboard_gtk_finalize(GObject *object)
{
  WPEClipboardGtk *clipboard_gtk = WPE_CLIPBOARD_GTK(object);
  g_signal_handlers_disconnect_by_data(clipboard_gtk->clipboard, clipboard_gtk);
  g_clear_object(&clipboard_gtk->clipboard);

  G_OBJECT_CLASS(wpe_clipboard_gtk_parent_class)->finalize(object);
}

typedef struct {
  GMainLoop *loop;
  GInputStream *in_stream;
} ReadAsyncData;

static void read_async_cb(GdkClipboard *clipboard, GAsyncResult *result, ReadAsyncData *data)
{
  GError *error = NULL;
  data->in_stream = gdk_clipboard_read_finish(clipboard, result, NULL, &error);
  g_main_loop_quit(data->loop);
}

static gboolean read_timeout_cb(GCancellable *cancellable)
{
  g_cancellable_cancel(cancellable);
  return G_SOURCE_REMOVE;
}

static GBytes *wpe_clipboard_gtk_read(WPEClipboard *clipboard, const char *format)
{
  WPEClipboardGtk *clipboard_gtk = WPE_CLIPBOARD_GTK(clipboard);

  GCancellable *cancellable = g_cancellable_new();
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  guint timeout_source_id = g_timeout_add(500, (GSourceFunc)read_timeout_cb, cancellable);

  ReadAsyncData data = { loop, NULL };
  const char* mimeTypes[] = { format, NULL };
  gdk_clipboard_read_async(clipboard_gtk->clipboard, mimeTypes, G_PRIORITY_DEFAULT, cancellable, (GAsyncReadyCallback)read_async_cb, &data);

  g_main_loop_run(loop);

  g_clear_handle_id(&timeout_source_id, g_source_remove);
  g_object_unref(cancellable);
  g_main_loop_unref(loop);

  if (!data.in_stream)
    return NULL;

  GOutputStream *out_stream = g_memory_output_stream_new_resizable();
  gssize result = g_output_stream_splice(out_stream, data.in_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, NULL, NULL);
  g_object_unref(data.in_stream);
  if (result == -1)
    return NULL;

  GBytes *bytes = g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(out_stream));
  g_object_unref(out_stream);
  return bytes;
}

static void wpe_clipboard_gtk_changed(WPEClipboard *clipboard, GPtrArray *formats, gboolean is_local, WPEClipboardContent *content)
{
  WPEClipboardGtk *clipboard_gtk = WPE_CLIPBOARD_GTK(clipboard);

  if (is_local) {
    GdkContentProvider *provider = NULL;
    if (formats) {
      GPtrArray *providers = g_ptr_array_sized_new(formats->len);
      gboolean textAdded = FALSE;
      for (unsigned i = 0; formats->pdata[i]; ++i) {
        const char* internal_format = g_intern_string(formats->pdata[i]);
        if (internal_format == g_intern_static_string("text/plain") || internal_format == g_intern_static_string("text/plain;charset=utf-8")) {
          if (!textAdded) {
            g_ptr_array_add(providers, gdk_content_provider_new_typed(G_TYPE_STRING, wpe_clipboard_content_get_text(content)));
            textAdded = TRUE;
          }
        } else {
          GBytes *bytes = wpe_clipboard_content_get_bytes(content, formats->pdata[i]);
          if (bytes)
            g_ptr_array_add(providers, gdk_content_provider_new_for_bytes(formats->pdata[i], bytes));
        }
      }

      provider = gdk_content_provider_new_union((GdkContentProvider **)providers->pdata, providers->len);
      g_ptr_array_unref(providers);
    }

    gdk_clipboard_set_content(clipboard_gtk->clipboard, provider);
    g_clear_object(&provider);
  }

  WPE_CLIPBOARD_CLASS(wpe_clipboard_gtk_parent_class)->changed(clipboard, formats, is_local, content);
}

static void wpe_clipboard_gtk_class_init(WPEClipboardGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = wpe_clipboard_gtk_finalize;

  WPEClipboardClass* clipboard_class = WPE_CLIPBOARD_CLASS(klass);
  clipboard_class->read = wpe_clipboard_gtk_read;
  clipboard_class->changed = wpe_clipboard_gtk_changed;
}

static void wpe_clipboard_gtk_init(WPEClipboardGtk *clipboard_gtk)
{
}

static void clipboard_changed_cb(GdkClipboard *clipboard, WPEClipboardGtk *clipboard_gtk)
{
  gsize n_types;
  const char* const* types = gdk_content_formats_get_mime_types(gdk_clipboard_get_formats(clipboard_gtk->clipboard), &n_types);
  if (n_types != 0) {
    GPtrArray* formats = g_ptr_array_sized_new(n_types);
    for (unsigned i = 0; types[i]; i++)
      g_ptr_array_add(formats, (gpointer)types[i]);
    g_ptr_array_add(formats, NULL);
    WPE_CLIPBOARD_GET_CLASS(clipboard_gtk)->changed(WPE_CLIPBOARD(clipboard_gtk), formats, FALSE, NULL);
    g_ptr_array_unref(formats);
  }
}

WPEClipboard *wpe_clipboard_gtk_new(GdkDisplay *display)
{
  g_return_val_if_fail(GDK_IS_DISPLAY(display), NULL);

  WPEClipboardGtk *clipboard_gtk = WPE_CLIPBOARD_GTK(g_object_new(WPE_TYPE_CLIPBOARD_GTK, NULL));
  clipboard_gtk->clipboard = g_object_ref(gdk_display_get_clipboard(display));

  clipboard_changed_cb(clipboard_gtk->clipboard, clipboard_gtk);
  g_signal_connect(clipboard_gtk->clipboard, "changed", G_CALLBACK(clipboard_changed_cb), clipboard_gtk);

  return WPE_CLIPBOARD(clipboard_gtk);
}
