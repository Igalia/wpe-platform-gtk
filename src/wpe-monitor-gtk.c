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

#include "wpe-monitor-gtk-private.h"

struct _WPEMonitorGtk {
  WPEMonitor parent;

  GdkMonitor *monitor;
};

G_DEFINE_FINAL_TYPE(WPEMonitorGtk, wpe_monitor_gtk, WPE_TYPE_MONITOR)

static void wpe_monitor_gtk_finalize(GObject *object)
{
  WPEMonitorGtk *monitor_gtk = WPE_MONITOR_GTK(object);
  g_clear_object(&monitor_gtk->monitor);

  G_OBJECT_CLASS(wpe_monitor_gtk_parent_class)->finalize(object);
}

static void wpe_monitor_gtk_class_init(WPEMonitorGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = wpe_monitor_gtk_finalize;
}

static void wpe_monitor_gtk_init(WPEMonitorGtk *monitor_gtk)
{
}

WPEMonitor *wpe_monitor_gtk_create(GdkMonitor *monitor)
{
  g_return_val_if_fail(GDK_IS_MONITOR(monitor), NULL);

  static guint monitor_id = 0;
  WPEMonitor *wpe_monitor = WPE_MONITOR(g_object_new(WPE_TYPE_MONITOR_GTK, "id", ++monitor_id, NULL));
  WPE_MONITOR_GTK(wpe_monitor)->monitor = g_object_ref(monitor);

  GdkRectangle geometry;
  gdk_monitor_get_geometry(monitor, &geometry);
  wpe_monitor_set_position(wpe_monitor, geometry.x, geometry.y);
  wpe_monitor_set_size(wpe_monitor, geometry.width, geometry.height);
  wpe_monitor_set_physical_size(wpe_monitor, gdk_monitor_get_width_mm(monitor), gdk_monitor_get_height_mm(monitor));
  wpe_monitor_set_scale(wpe_monitor, gdk_monitor_get_scale_factor(monitor));
  wpe_monitor_set_refresh_rate(wpe_monitor, gdk_monitor_get_refresh_rate(monitor));

  return wpe_monitor;
}

GdkMonitor *wpe_monitor_gtk_get_gdk_monitor(WPEMonitorGtk *monitor_gtk)
{
  g_return_val_if_fail(WPE_IS_MONITOR_GTK(monitor_gtk), NULL);

  return monitor_gtk->monitor;
}
