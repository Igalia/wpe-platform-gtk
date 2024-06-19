/*
 * Copyright (c) 2024 Igalia S.L.
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

#include "wpe-view-gtk.h"

#include "wpe-drawing-area.h"
#include "wpe-monitor-gtk.h"
#include "wpe-toplevel-gtk.h"

struct _WPEViewGtk {
  WPEView parent;

  WPEDrawingArea *drawing_area;
};

G_DEFINE_FINAL_TYPE(WPEViewGtk, wpe_view_gtk, WPE_TYPE_VIEW)

static void wpe_view_gtk_monitor_changed(WPEView *view, GParamSpec *pspec, gpointer user_data)
{
  if (wpe_view_get_monitor(view))
    wpe_view_map(view);
  else
    wpe_view_unmap(view);
}

static void wpe_view_gtk_constructed(GObject *object)
{
  G_OBJECT_CLASS(wpe_view_gtk_parent_class)->constructed(object);

  WPEViewGtk *view_gtk = WPE_VIEW_GTK(object);
  g_signal_connect(view_gtk, "notify::monitor", G_CALLBACK(wpe_view_gtk_monitor_changed), NULL);
}

static void wpe_view_gtk_finalize(GObject *object)
{
  WPEViewGtk *view_gtk = WPE_VIEW_GTK(object);
  if (view_gtk->drawing_area)
    g_object_remove_weak_pointer(G_OBJECT(view_gtk->drawing_area), (gpointer*)&view_gtk->drawing_area);

  G_OBJECT_CLASS(wpe_view_gtk_parent_class)->finalize(object);
}

static gboolean wpe_view_gtk_render_buffer(WPEView *view, WPEBuffer *buffer, const WPERectangle *damage_rects, guint n_damage_rects, GError **error)
{
  WPEViewGtk *view_gtk = WPE_VIEW_GTK(view);
  if (!view_gtk->drawing_area) {
    g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: drawing area was destroyed");
    return FALSE;
  }

  return wpe_drawing_area_render_buffer(view_gtk->drawing_area, buffer, damage_rects, n_damage_rects, error);
}

static void wpe_view_gtk_set_cursor_from_name(WPEView *view, const char *name)
{
  WPEViewGtk *view_gtk = WPE_VIEW_GTK(view);
  if (!view_gtk->drawing_area)
    return;

  gtk_widget_set_cursor_from_name(GTK_WIDGET(view_gtk->drawing_area), name);
}

static void wpe_view_gtk_set_cursor_from_bytes(WPEView *view, GBytes *bytes, guint width, guint height, guint stride, guint hotspot_x, guint hotspot_y)
{
  WPEViewGtk *view_gtk = WPE_VIEW_GTK(view);
  if (!view_gtk->drawing_area)
    return;

  g_autoptr(GdkTexture) texture = gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT, bytes, stride);
  g_autoptr(GdkCursor) cursor = gdk_cursor_new_from_texture(texture, hotspot_x, hotspot_y, NULL);
  gtk_widget_set_cursor(GTK_WIDGET(view_gtk->drawing_area), cursor);
}

static void wpe_view_gtk_set_opaque_rectangles(WPEView *view, WPERectangle *rects, guint n_rects)
{
  WPEViewGtk *view_gtk = WPE_VIEW_GTK(view);
  if (!view_gtk->drawing_area)
    return;

  GdkSurface *surface = gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(view_gtk->drawing_area)));
  if (!surface)
    return;

  cairo_region_t *region = NULL;
  if (rects) {
    region = cairo_region_create();
    for (guint i = 0; i < n_rects; i++) {
      GdkRectangle rect = { rects[i].x, rects[i].y, rects[i].width, rects[i].height };
      cairo_region_union_rectangle(region, &rect);
    }
  }

  gdk_surface_set_opaque_region(surface, region);
  if (region)
    cairo_region_destroy(region);
}

static gboolean wpe_view_gtk_can_be_mapped(WPEView *view)
{
  WPEViewGtk *view_gtk = WPE_VIEW_GTK(view);
  if (!view_gtk->drawing_area || !gtk_widget_get_mapped(GTK_WIDGET(view_gtk->drawing_area)))
    return FALSE;

  WPEToplevel *toplevel = wpe_view_get_toplevel(view);
  return toplevel ? wpe_toplevel_gtk_is_in_monitor(WPE_TOPLEVEL_GTK(toplevel)) : FALSE;
}

static void wpe_view_gtk_class_init(WPEViewGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->constructed = wpe_view_gtk_constructed;
  object_class->finalize = wpe_view_gtk_finalize;

  WPEViewClass *view_class = WPE_VIEW_CLASS(klass);
  view_class->render_buffer = wpe_view_gtk_render_buffer;
  view_class->set_cursor_from_name = wpe_view_gtk_set_cursor_from_name;
  view_class->set_cursor_from_bytes = wpe_view_gtk_set_cursor_from_bytes;
  view_class->set_opaque_rectangles = wpe_view_gtk_set_opaque_rectangles;
  view_class->can_be_mapped = wpe_view_gtk_can_be_mapped;
}

static void wpe_view_gtk_init(WPEViewGtk *view_gtk)
{
  view_gtk->drawing_area = WPE_DRAWING_AREA(wpe_drawing_area_new(WPE_VIEW(view_gtk)));
  g_object_add_weak_pointer(G_OBJECT(view_gtk->drawing_area), (gpointer*)&view_gtk->drawing_area);
}

WPEView *wpe_view_gtk_new(WPEDisplayGtk *display)
{
  g_return_val_if_fail(WPE_IS_DISPLAY_GTK(display), NULL);

  return g_object_new(WPE_TYPE_VIEW_GTK, "display", display, NULL);
}

GtkWidget *wpe_view_gtk_get_widget(WPEViewGtk *view)
{
  g_return_val_if_fail(WPE_IS_VIEW_GTK(view), NULL);

  return GTK_WIDGET(view->drawing_area);
}
