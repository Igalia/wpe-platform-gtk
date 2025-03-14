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

#include "wpe-input-method-context-gtk.h"

#include "wpe-view-gtk.h"
#include <gtk/gtk.h>

struct _WPEInputMethodContextGtk {
  WPEInputMethodContext parent;

  GtkIMContext *im_context;
  gchar *surrounding_text;
  guint surrounding_cursor_index;
  guint surrounding_selection_index;
};

G_DEFINE_FINAL_TYPE(WPEInputMethodContextGtk, wpe_input_method_context_gtk, WPE_TYPE_INPUT_METHOD_CONTEXT)

static void input_purpose_changed_cb(WPEInputMethodContextGtk *context_gtk)
{
  GtkInputPurpose gtk_purpose;
  switch (wpe_input_method_context_get_input_purpose(WPE_INPUT_METHOD_CONTEXT(context_gtk))) {
  case WPE_INPUT_PURPOSE_FREE_FORM:
    gtk_purpose = GTK_INPUT_PURPOSE_FREE_FORM;
    break;
  case WPE_INPUT_PURPOSE_ALPHA:
    gtk_purpose = GTK_INPUT_PURPOSE_ALPHA;
    break;
  case WPE_INPUT_PURPOSE_DIGITS:
    gtk_purpose = GTK_INPUT_PURPOSE_DIGITS;
    break;
  case WPE_INPUT_PURPOSE_NUMBER:
    gtk_purpose = GTK_INPUT_PURPOSE_NUMBER;
    break;
  case WPE_INPUT_PURPOSE_PHONE:
    gtk_purpose = GTK_INPUT_PURPOSE_PHONE;
    break;
  case WPE_INPUT_PURPOSE_URL:
    gtk_purpose = GTK_INPUT_PURPOSE_URL;
    break;
  case WPE_INPUT_PURPOSE_EMAIL:
    gtk_purpose = GTK_INPUT_PURPOSE_EMAIL;
    break;
  case WPE_INPUT_PURPOSE_NAME:
    gtk_purpose = GTK_INPUT_PURPOSE_NAME;
    break;
  case WPE_INPUT_PURPOSE_PASSWORD:
    gtk_purpose = GTK_INPUT_PURPOSE_PASSWORD;
    break;
  case WPE_INPUT_PURPOSE_PIN:
    gtk_purpose = GTK_INPUT_PURPOSE_PIN;
    break;
  case WPE_INPUT_PURPOSE_TERMINAL:
    gtk_purpose = GTK_INPUT_PURPOSE_TERMINAL;
    break;
  }

  g_object_set(context_gtk->im_context, "input-purpose", gtk_purpose, NULL);
}

static void input_hints_changed_cb(WPEInputMethodContextGtk *context_gtk)
{
  WPEInputHints hints = wpe_input_method_context_get_input_hints(WPE_INPUT_METHOD_CONTEXT(context_gtk));
  GtkInputHints gtk_hints = GTK_INPUT_HINT_NONE;
  if (hints & WPE_INPUT_HINT_SPELLCHECK)
    gtk_hints |= GTK_INPUT_HINT_SPELLCHECK;
  if (hints & WPE_INPUT_HINT_NO_SPELLCHECK)
    gtk_hints |= GTK_INPUT_HINT_NO_SPELLCHECK;
  if (hints & WPE_INPUT_HINT_WORD_COMPLETION)
    gtk_hints |= GTK_INPUT_HINT_WORD_COMPLETION;
  if (hints & WPE_INPUT_HINT_LOWERCASE)
    gtk_hints |= GTK_INPUT_HINT_LOWERCASE;
  if (hints & WPE_INPUT_HINT_UPPERCASE_CHARS)
    gtk_hints |= GTK_INPUT_HINT_UPPERCASE_CHARS;
  if (hints & WPE_INPUT_HINT_UPPERCASE_WORDS)
    gtk_hints |= GTK_INPUT_HINT_UPPERCASE_WORDS;
  if (hints & WPE_INPUT_HINT_UPPERCASE_SENTENCES)
    gtk_hints |= GTK_INPUT_HINT_UPPERCASE_SENTENCES;
  if (hints & WPE_INPUT_HINT_INHIBIT_OSK)
    gtk_hints |= GTK_INPUT_HINT_INHIBIT_OSK;
  if (hints & WPE_INPUT_HINT_VERTICAL_WRITING)
    gtk_hints |= GTK_INPUT_HINT_VERTICAL_WRITING;
  if (hints & WPE_INPUT_HINT_EMOJI)
    gtk_hints |= GTK_INPUT_HINT_EMOJI;
  if (hints & WPE_INPUT_HINT_NO_EMOJI)
    gtk_hints |= GTK_INPUT_HINT_NO_EMOJI;
  if (hints & WPE_INPUT_HINT_PRIVATE)
    gtk_hints |= GTK_INPUT_HINT_PRIVATE;

  g_object_set(context_gtk->im_context, "input-hints", gtk_hints, NULL);
}

static void im_context_preedit_start_cb(WPEInputMethodContextGtk *context_gtk)
{
  g_signal_emit_by_name(context_gtk, "preedit-started", NULL);
}

static void im_context_preedit_changed_cb(WPEInputMethodContextGtk *context_gtk)
{
  g_signal_emit_by_name(context_gtk, "preedit-changed", NULL);
}

static void im_context_preedit_end_cb(WPEInputMethodContextGtk *context_gtk)
{
  g_signal_emit_by_name(context_gtk, "preedit-finished", NULL);
}

static void im_context_commit_cb(WPEInputMethodContextGtk *context_gtk, const char* text)
{
  g_signal_emit_by_name(context_gtk, "committed", text, NULL);
}

static void im_context_retrieve_surrounding_cb(WPEInputMethodContextGtk *context_gtk)
{
  gtk_im_context_set_surrounding_with_selection(context_gtk->im_context,
                                                context_gtk->surrounding_text,
                                                context_gtk->surrounding_text ? -1 : 0,
                                                context_gtk->surrounding_cursor_index,
                                                context_gtk->surrounding_selection_index);
}

static void im_context_delete_surrounding_cb(WPEInputMethodContextGtk *context_gtk, int offset, int n_chars)
{
  gtk_im_context_delete_surrounding(context_gtk->im_context, offset, n_chars);
}

static void client_widget_realize_cb(WPEInputMethodContextGtk *context_gtk, GtkWidget* widget)
{
  gtk_im_context_set_client_widget(context_gtk->im_context, widget);
}

static void client_widget_unrealize_cb(WPEInputMethodContextGtk *context_gtk, GtkWidget* widget)
{
  gtk_im_context_set_client_widget(context_gtk->im_context, NULL);
}

static void wpe_input_method_context_gtk_constructed(GObject *object)
{
  G_OBJECT_CLASS(wpe_input_method_context_gtk_parent_class)->constructed(object);

  g_signal_connect(object, "notify::input-purpose", G_CALLBACK(input_purpose_changed_cb), NULL);
  g_signal_connect(object, "notify::input-hints", G_CALLBACK(input_hints_changed_cb), NULL);

  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(object);
  context_gtk->im_context = gtk_im_multicontext_new();
  g_signal_connect_object(context_gtk->im_context, "preedit-start", G_CALLBACK(im_context_preedit_start_cb), object, G_CONNECT_SWAPPED);
  g_signal_connect_object(context_gtk->im_context, "preedit-changed", G_CALLBACK(im_context_preedit_changed_cb), object, G_CONNECT_SWAPPED);
  g_signal_connect_object(context_gtk->im_context, "preedit-end", G_CALLBACK(im_context_preedit_end_cb), object, G_CONNECT_SWAPPED);
  g_signal_connect_object(context_gtk->im_context, "commit", G_CALLBACK(im_context_commit_cb), object, G_CONNECT_SWAPPED);
  g_signal_connect_object(context_gtk->im_context, "retrieve-surrounding", G_CALLBACK(im_context_retrieve_surrounding_cb), object, G_CONNECT_SWAPPED);
  g_signal_connect_object(context_gtk->im_context, "delete-surrounding", G_CALLBACK(im_context_delete_surrounding_cb), object, G_CONNECT_SWAPPED);

  WPEView *view	= wpe_input_method_context_get_view(WPE_INPUT_METHOD_CONTEXT(object));
  GtkWidget *widget = wpe_view_gtk_get_widget(WPE_VIEW_GTK(view));
  g_signal_connect_object(widget, "realize", G_CALLBACK(client_widget_realize_cb), object, G_CONNECT_SWAPPED);
  g_signal_connect_object(widget, "unrealize", G_CALLBACK(client_widget_unrealize_cb), object, G_CONNECT_SWAPPED);
  if (gtk_widget_get_realized(widget))
    gtk_im_context_set_client_widget(context_gtk->im_context, wpe_view_gtk_get_widget(WPE_VIEW_GTK(view)));
}

static void wpe_input_method_context_gtk_finalize(GObject *object)
{
  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(object);
  g_clear_object(&context_gtk->im_context);
  g_free(context_gtk->surrounding_text);

  G_OBJECT_CLASS(wpe_input_method_context_gtk_parent_class)->finalize(object);
}

static void wpe_input_method_context_gtk_get_preedit_string(WPEInputMethodContext *context, gchar **text, GList **underlines, guint *cursor_offset)
{
  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(context);

  PangoAttrList* attr_list = NULL;
  int offset;
  gtk_im_context_get_preedit_string(context_gtk->im_context, text, underlines ? &attr_list : NULL, &offset);

  if (underlines) {
    *underlines = NULL;
    if (attr_list) {
      PangoAttrIterator *iter = pango_attr_list_get_iterator(attr_list);

      do {
        if (!pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE))
          continue;

        int start, end;
        pango_attr_iterator_range(iter, &start, &end);

        WPEInputMethodUnderline *underline = wpe_input_method_underline_new(start, end);
        PangoAttribute *color_attr = pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE_COLOR);
        if (color_attr) {
          PangoColor *color = &((PangoAttrColor*)color_attr)->color;
          WPEColor rgba = { color->red / 65535.f, color->green / 65535.f, color->blue / 65535.f, 1.f };
          wpe_input_method_underline_set_color(underline, &rgba);
        }

	*underlines = g_list_prepend(*underlines, underline);
      } while (pango_attr_iterator_next(iter));
    }
  }

  g_clear_pointer(&attr_list, pango_attr_list_unref);

  if (cursor_offset)
    *cursor_offset = offset;
}

static gboolean wpe_input_method_context_gtk_filter_key_event(WPEInputMethodContext *context, WPEEvent *event)
{
  gpointer gdk_event = wpe_event_get_user_data(event);
  if (!GDK_IS_EVENT(gdk_event))
    return FALSE;

  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(context);
  return gtk_im_context_filter_keypress(context_gtk->im_context, GDK_EVENT(gdk_event));
}

static void wpe_input_method_context_gtk_focus_in(WPEInputMethodContext *context)
{
  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(context);
  gtk_im_context_focus_in(context_gtk->im_context);
}

static void wpe_input_method_context_gtk_focus_out(WPEInputMethodContext *context)
{
  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(context);
  gtk_im_context_focus_out(context_gtk->im_context);
}

static void wpe_input_method_context_gtk_set_cursor_area(WPEInputMethodContext *context, int x, int y, int width, int height)
{
  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(context);
  GdkRectangle cursor_rect = { x, y, width, height };
  gtk_im_context_set_cursor_location(context_gtk->im_context, &cursor_rect);
}

static void wpe_input_method_context_gtk_set_surrounding(WPEInputMethodContext *context, const gchar *text, guint length, guint cursor_index, guint selection_index)
{
  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(context);
  g_free(context_gtk->surrounding_text);
  context_gtk->surrounding_text = g_strndup(text, length);
  context_gtk->surrounding_cursor_index = cursor_index;
  context_gtk->surrounding_selection_index = selection_index;
}

static void wpe_input_method_context_gtk_reset(WPEInputMethodContext *context)
{
  WPEInputMethodContextGtk *context_gtk = WPE_INPUT_METHOD_CONTEXT_GTK(context);
  gtk_im_context_reset(context_gtk->im_context);
}

static void wpe_input_method_context_gtk_class_init(WPEInputMethodContextGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->constructed = wpe_input_method_context_gtk_constructed;
  object_class->finalize = wpe_input_method_context_gtk_finalize;

  WPEInputMethodContextClass *im_context_class = WPE_INPUT_METHOD_CONTEXT_CLASS(klass);
  im_context_class->get_preedit_string = wpe_input_method_context_gtk_get_preedit_string;
  im_context_class->filter_key_event = wpe_input_method_context_gtk_filter_key_event;
  im_context_class->focus_in = wpe_input_method_context_gtk_focus_in;
  im_context_class->focus_out = wpe_input_method_context_gtk_focus_out;
  im_context_class->set_cursor_area = wpe_input_method_context_gtk_set_cursor_area;
  im_context_class->set_surrounding = wpe_input_method_context_gtk_set_surrounding;
  im_context_class->reset = wpe_input_method_context_gtk_reset;
}

static void wpe_input_method_context_gtk_init(WPEInputMethodContextGtk *context_gtk)
{
}

WPEInputMethodContext *wpe_input_method_context_gtk_new(WPEView *view)
{
  return WPE_INPUT_METHOD_CONTEXT(g_object_new(WPE_TYPE_INPUT_METHOD_CONTEXT_GTK, "view", view, NULL));
}
