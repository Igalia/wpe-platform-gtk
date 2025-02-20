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

#include "wpe-screen-gtk-private.h"

struct _WPEScreenGtk {
  WPEScreen parent;

  GdkMonitor *monitor;
};

G_DEFINE_FINAL_TYPE(WPEScreenGtk, wpe_screen_gtk, WPE_TYPE_SCREEN)

static void wpe_screen_gtk_finalize(GObject *object)
{
  WPEScreenGtk *screen_gtk = WPE_SCREEN_GTK(object);
  g_clear_object(&screen_gtk->monitor);

  G_OBJECT_CLASS(wpe_screen_gtk_parent_class)->finalize(object);
}

static void wpe_screen_gtk_class_init(WPEScreenGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = wpe_screen_gtk_finalize;
}

static void wpe_screen_gtk_init(WPEScreenGtk *screen_gtk)
{
}

WPEScreen *wpe_screen_gtk_create(GdkMonitor *monitor)
{
  g_return_val_if_fail(GDK_IS_MONITOR(monitor), NULL);

  static guint screen_id = 0;
  WPEScreen *screen = WPE_SCREEN(g_object_new(WPE_TYPE_SCREEN_GTK, "id", ++screen_id, NULL));
  WPE_SCREEN_GTK(screen)->monitor = g_object_ref(monitor);

  GdkRectangle geometry;
  gdk_monitor_get_geometry(monitor, &geometry);
  wpe_screen_set_position(screen, geometry.x, geometry.y);
  wpe_screen_set_size(screen, geometry.width, geometry.height);
  wpe_screen_set_physical_size(screen, gdk_monitor_get_width_mm(monitor), gdk_monitor_get_height_mm(monitor));
  wpe_screen_set_scale(screen, gdk_monitor_get_scale_factor(monitor));
  wpe_screen_set_refresh_rate(screen, gdk_monitor_get_refresh_rate(monitor));

  return screen;
}

GdkMonitor *wpe_screen_gtk_get_gdk_monitor(WPEScreenGtk *screen)
{
  g_return_val_if_fail(WPE_IS_SCREEN_GTK(screen), NULL);

  return screen->monitor;
}
