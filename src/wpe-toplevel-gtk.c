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

#include "wpe-toplevel-gtk.h"

#include "wpe-drawing-area.h"
#include "wpe-screen-gtk.h"

enum {
  PROP_0,

  PROP_WINDOW,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

struct _WPEToplevelGtk {
  WPEToplevel parent;

  GtkWindow *window;
  GdkToplevelState state;
  GList *monitors;
  GdkMonitor *current_monitor;
};

G_DEFINE_FINAL_TYPE(WPEToplevelGtk, wpe_toplevel_gtk, WPE_TYPE_TOPLEVEL)

static void wpe_toplevel_gtk_state_changed(GdkSurface *surface, GParamSpec *pspec, WPEToplevelGtk *toplevel_gtk)
{
  GdkToplevelState toplevel_state = gdk_toplevel_get_state(GDK_TOPLEVEL(surface));
  int mask = toplevel_gtk->state ^ toplevel_state;
  toplevel_gtk->state = toplevel_state;

  WPEToplevelState state = wpe_toplevel_get_state(WPE_TOPLEVEL(toplevel_gtk));
  if (mask & GDK_TOPLEVEL_STATE_FULLSCREEN) {
    if (toplevel_state & GDK_TOPLEVEL_STATE_FULLSCREEN)
      state |= WPE_TOPLEVEL_STATE_FULLSCREEN;
    else
      state &= ~WPE_TOPLEVEL_STATE_FULLSCREEN;
  }

  if (mask & GDK_TOPLEVEL_STATE_MAXIMIZED) {
    if (toplevel_state & GDK_TOPLEVEL_STATE_MAXIMIZED)
      state |= WPE_TOPLEVEL_STATE_MAXIMIZED;
    else
      state &= ~WPE_TOPLEVEL_STATE_MAXIMIZED;
  }

  if (mask & GDK_TOPLEVEL_STATE_FOCUSED) {
    if (toplevel_state & GDK_TOPLEVEL_STATE_FOCUSED)
      state |= WPE_TOPLEVEL_STATE_ACTIVE;
    else
      state &= ~WPE_TOPLEVEL_STATE_ACTIVE;
  }

  wpe_toplevel_state_changed(WPE_TOPLEVEL(toplevel_gtk), state);
}

static void wpe_toplevel_gtk_entered_monitor(GdkSurface *surface, GdkMonitor *monitor, WPEToplevelGtk *toplevel_gtk)
{
  toplevel_gtk->monitors = g_list_append(toplevel_gtk->monitors, monitor);
  if (toplevel_gtk->current_monitor != monitor) {
    toplevel_gtk->current_monitor = monitor;
    wpe_toplevel_screen_changed(WPE_TOPLEVEL(toplevel_gtk));
  }
}

static void wpe_toplevel_gtk_left_monitor(GdkSurface *surface, GdkMonitor *monitor, WPEToplevelGtk *toplevel_gtk)
{
  toplevel_gtk->monitors = g_list_remove(toplevel_gtk->monitors, monitor);

  GdkMonitor *current_monitor = NULL;
  if (toplevel_gtk->monitors)
    current_monitor = gdk_display_get_monitor_at_surface(gtk_widget_get_display(GTK_WIDGET(toplevel_gtk->window)), surface);
  if (toplevel_gtk->current_monitor != current_monitor) {
    toplevel_gtk->current_monitor = current_monitor;
    wpe_toplevel_screen_changed(WPE_TOPLEVEL(toplevel_gtk));
  }
}

static void wpe_toplevel_gtk_connect_surface_signals(WPEToplevelGtk *toplevel_gtk)
{
  GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(toplevel_gtk->window));
  wpe_toplevel_gtk_state_changed(surface, NULL, toplevel_gtk);
  g_signal_connect(surface, "notify::state", G_CALLBACK(wpe_toplevel_gtk_state_changed), toplevel_gtk);

  toplevel_gtk->current_monitor = gdk_display_get_monitor_at_surface(gtk_widget_get_display(GTK_WIDGET(toplevel_gtk->window)), surface);
  if (toplevel_gtk->current_monitor)
    wpe_toplevel_screen_changed(WPE_TOPLEVEL(toplevel_gtk));
  g_signal_connect(surface, "enter-monitor", G_CALLBACK(wpe_toplevel_gtk_entered_monitor), toplevel_gtk);
  g_signal_connect(surface, "leave-monitor", G_CALLBACK(wpe_toplevel_gtk_left_monitor), toplevel_gtk);
}

static void wpe_toplevel_gtk_disconnect_surface_signals(WPEToplevelGtk *toplevel_gtk)
{
  GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(toplevel_gtk->window));
  g_signal_handlers_disconnect_by_data(surface, toplevel_gtk);
}

static void wpe_toplevel_gtk_realized(WPEToplevelGtk *toplevel_gtk)
{
  wpe_toplevel_gtk_connect_surface_signals(toplevel_gtk);
}

static void wpe_toplevel_gtk_unrealized(WPEToplevelGtk *toplevel_gtk)
{
  wpe_toplevel_gtk_disconnect_surface_signals(toplevel_gtk);
}

static void wpe_toplevel_gtk_scale_changed(WPEToplevelGtk *toplevel_gtk)
{
  wpe_toplevel_scale_changed(WPE_TOPLEVEL(toplevel_gtk), gtk_widget_get_scale_factor(GTK_WIDGET(toplevel_gtk->window)));
}

static void wpe_toplevel_gtk_constructed(GObject *object)
{
  G_OBJECT_CLASS(wpe_toplevel_gtk_parent_class)->constructed(object);

  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(object);
  if (gtk_widget_get_realized(GTK_WIDGET(toplevel_gtk->window)))
    wpe_toplevel_gtk_connect_surface_signals(toplevel_gtk);
  else
    g_signal_connect_swapped(toplevel_gtk->window, "realize", G_CALLBACK(wpe_toplevel_gtk_realized), toplevel_gtk);
  g_signal_connect_swapped(toplevel_gtk->window, "unrealize", G_CALLBACK(wpe_toplevel_gtk_unrealized), toplevel_gtk);
  wpe_toplevel_scale_changed(WPE_TOPLEVEL(toplevel_gtk), gtk_widget_get_scale_factor(GTK_WIDGET(toplevel_gtk->window)));
  g_signal_connect_swapped(toplevel_gtk->window, "notify::scale-factor", G_CALLBACK(wpe_toplevel_gtk_scale_changed), toplevel_gtk);
}

static void wpe_toplevel_gtk_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(object);
  switch (prop_id) {
  case PROP_WINDOW:
    toplevel_gtk->window = g_value_get_object(value);
    if (toplevel_gtk->window)
      g_object_add_weak_pointer(G_OBJECT(toplevel_gtk->window), (gpointer*)&toplevel_gtk->window);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void wpe_toplevel_gtk_dispose(GObject *object)
{
  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(object);

  wpe_toplevel_closed(WPE_TOPLEVEL(toplevel_gtk));

  G_OBJECT_CLASS(wpe_toplevel_gtk_parent_class)->dispose(object);
}

static void wpe_toplevel_gtk_finalize(GObject *object)
{
  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(object);
  if (toplevel_gtk->window) {
    g_signal_handlers_disconnect_by_data(toplevel_gtk->window, toplevel_gtk);
    if (gtk_widget_get_realized(GTK_WIDGET(toplevel_gtk->window)))
      wpe_toplevel_gtk_disconnect_surface_signals(toplevel_gtk);
    g_object_remove_weak_pointer(G_OBJECT(toplevel_gtk->window), (gpointer*)&toplevel_gtk->window);
  }

  G_OBJECT_CLASS(wpe_toplevel_gtk_parent_class)->finalize(object);
}

static void wpe_toplevel_gtk_set_title(WPEToplevel *toplevel, const char *title)
{
  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(toplevel);
  if (toplevel_gtk->window)
    gtk_window_set_title(toplevel_gtk->window, title);
}

static WPEScreen *wpe_toplevel_gtk_get_screen(WPEToplevel *toplevel)
{
  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(toplevel);
  if (!toplevel_gtk->window)
    return NULL;

  if (!toplevel_gtk->current_monitor)
    return NULL;

  WPEDisplay *display = wpe_toplevel_get_display(toplevel);
  guint n_screens = wpe_display_get_n_screens(display);
  for (guint i = 0; i < n_screens; i++) {
    WPEScreen *screen = wpe_display_get_screen(display, i);
    if (wpe_screen_gtk_get_gdk_monitor(WPE_SCREEN_GTK(screen)) == toplevel_gtk->current_monitor)
      return screen;
  }

  return NULL;
}

static gboolean wpe_toplevel_gtk_set_fullscreen(WPEToplevel *toplevel, gboolean fullscreen)
{
  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(toplevel);
  if (!toplevel_gtk->window)
    return FALSE;

  if (fullscreen)
    gtk_window_fullscreen(toplevel_gtk->window);
  else
    gtk_window_unfullscreen(toplevel_gtk->window);

  return TRUE;
}

static gboolean wpe_toplevel_gtk_set_maximized(WPEToplevel *toplevel, gboolean maximized)
{
  WPEToplevelGtk *toplevel_gtk = WPE_TOPLEVEL_GTK(toplevel);
  if (!toplevel_gtk->window)
    return FALSE;

  if (maximized)
    gtk_window_maximize(toplevel_gtk->window);
  else
    gtk_window_unmaximize(toplevel_gtk->window);

  return TRUE;
}

static void wpe_toplevel_gtk_class_init(WPEToplevelGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->constructed = wpe_toplevel_gtk_constructed;
  object_class->set_property = wpe_toplevel_gtk_set_property;
  object_class->dispose = wpe_toplevel_gtk_dispose;
  object_class->finalize = wpe_toplevel_gtk_finalize;

  properties[PROP_WINDOW] =
    g_param_spec_object("window",
                        NULL, NULL,
                        GTK_TYPE_WINDOW,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties(object_class, N_PROPS, properties);

  WPEToplevelClass *toplevel_class = WPE_TOPLEVEL_CLASS(klass);
  toplevel_class->set_title = wpe_toplevel_gtk_set_title;
  toplevel_class->get_screen = wpe_toplevel_gtk_get_screen;
  toplevel_class->set_fullscreen = wpe_toplevel_gtk_set_fullscreen;
  toplevel_class->set_maximized = wpe_toplevel_gtk_set_maximized;
}

static void wpe_toplevel_gtk_init(WPEToplevelGtk *toplevel_gtk)
{
}

WPEToplevel *wpe_toplevel_gtk_new(WPEDisplayGtk *display, GtkWindow *window)
{
  g_return_val_if_fail(WPE_IS_DISPLAY_GTK(display), NULL);
  g_return_val_if_fail(GTK_IS_WINDOW(window), NULL);

  return g_object_new(WPE_TYPE_TOPLEVEL_GTK,
                      "display", display,
                      "window", window,
                      NULL);
}

GtkWindow *wpe_toplevel_gtk_get_window(WPEToplevelGtk *toplevel_gtk)
{
  g_return_val_if_fail(WPE_IS_TOPLEVEL_GTK(toplevel_gtk), NULL);

  return toplevel_gtk->window;
}

gboolean wpe_toplevel_gtk_is_in_screen(WPEToplevelGtk *toplevel_gtk)
{
  g_return_val_if_fail(WPE_IS_TOPLEVEL_GTK(toplevel_gtk), FALSE);

  return !!toplevel_gtk->current_monitor;
}
