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

#include "wpe-display-gtk.h"

#include "wpe-keymap-gtk.h"
#include "wpe-screen-gtk-private.h"
#include "wpe-view-gtk.h"
#include <epoxy/egl.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

struct _WPEDisplayGtk {
  WPEDisplay parent;

  GdkDisplay *display;
  EGLDisplay egl_display;
  char *drm_device;
  char *drm_render_node;
  WPEKeymap *keymap;
  GPtrArray *screens;

  GSettings *desktop_settings;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED(WPEDisplayGtk, wpe_display_gtk, WPE_TYPE_DISPLAY, G_TYPE_FLAG_FINAL, {})

static void wpe_display_gtk_finalize(GObject *object)
{
  WPEDisplayGtk *display_gtk = WPE_DISPLAY_GTK(object);
  g_clear_pointer(&display_gtk->drm_device, g_free);
  g_clear_pointer(&display_gtk->drm_render_node, g_free);
  g_clear_pointer(&display_gtk->screens, g_ptr_array_unref);
  g_clear_object(&display_gtk->keymap);
  g_clear_object(&display_gtk->desktop_settings);

  G_OBJECT_CLASS(wpe_display_gtk_parent_class)->finalize(object);
}

static void wpe_display_gtk_monitors_changed(WPEDisplayGtk *display_gtk, guint index, guint n_removed, guint n_added, GListModel *monitors)
{
  for (guint i = 0; i < n_removed; i++)
    g_ptr_array_remove_index(display_gtk->screens, index);

  for (guint i = 0; i < n_added; i++) {
    GdkMonitor *monitor = GDK_MONITOR(g_list_model_get_item(monitors, index + i));
    g_ptr_array_add(display_gtk->screens, wpe_screen_gtk_create(monitor));
    g_object_unref(monitor);
  }
}

static void wpe_display_gtk_setup_screens(WPEDisplayGtk *display_gtk)
{
  GListModel *monitors = gdk_display_get_monitors(display_gtk->display);
  guint n_monitors = g_list_model_get_n_items(monitors);
  display_gtk->screens = g_ptr_array_new_full(n_monitors, g_object_unref);
  for (guint i = 0; i < n_monitors; i++) {
    GdkMonitor *monitor = GDK_MONITOR(g_list_model_get_item(monitors, i));
    g_ptr_array_add(display_gtk->screens, wpe_screen_gtk_create(monitor));
    g_object_unref(monitor);
  }
  g_signal_connect_object(monitors, "items-changed", G_CALLBACK(wpe_display_gtk_monitors_changed), display_gtk, G_CONNECT_SWAPPED);
}

static WPESettingsHintingStyle wpe_font_hinting_style(const char *hinting_style)
{
  if (g_strcmp0(hinting_style, "hintnone") == 0)
    return WPE_SETTINGS_HINTING_STYLE_NONE;
  if (g_strcmp0(hinting_style, "hintslight") == 0)
    return WPE_SETTINGS_HINTING_STYLE_SLIGHT;
  if (g_strcmp0(hinting_style, "hintmedium") == 0)
    return WPE_SETTINGS_HINTING_STYLE_MEDIUM;
  if (g_strcmp0(hinting_style, "hintfull") == 0)
    return WPE_SETTINGS_HINTING_STYLE_FULL;
  return WPE_SETTINGS_HINTING_STYLE_SLIGHT;
}

static WPESettingsSubpixelLayout wpe_subpixel_layout(const char *subpixel_layout)
{
  if (g_strcmp0(subpixel_layout, "rgb") == 0)
    return WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB;
  if (g_strcmp0(subpixel_layout, "bgr") == 0)
    return WPE_SETTINGS_SUBPIXEL_LAYOUT_BGR;
  if (g_strcmp0(subpixel_layout, "vrgb") == 0)
    return WPE_SETTINGS_SUBPIXEL_LAYOUT_VRGB;
  if (g_strcmp0(subpixel_layout, "vbgr") == 0)
    return WPE_SETTINGS_SUBPIXEL_LAYOUT_VBGR;
  return WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB;
}

static void color_scheme_settings_changed(GSettings *desktop_settings)
{
  gboolean use_dark_mode = g_settings_get_enum(desktop_settings, "color-scheme") == 1;
  g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", use_dark_mode, NULL);
}

static void wpe_display_gtk_setup_dark_mode(WPEDisplayGtk *display_gtk)
{
  if (g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS))
    return;

  g_autoptr(GSettingsSchema) schema = g_settings_schema_source_lookup(g_settings_schema_source_get_default(), "org.gnome.desktop.interface", TRUE);
  if (!schema || !g_settings_schema_has_key(schema, "color-scheme"))
    return;

  display_gtk->desktop_settings = g_settings_new("org.gnome.desktop.interface");
  color_scheme_settings_changed(display_gtk->desktop_settings);
  g_signal_connect(display_gtk->desktop_settings, "changed::color-scheme", G_CALLBACK(color_scheme_settings_changed), NULL);
}

static void gtk_settings_changed (WPEDisplayGtk *display_gtk)
{
  WPESettings *settings = wpe_display_get_settings(WPE_DISPLAY(display_gtk));
  GtkSettings *gtk_settings = gtk_settings_get_default();

  int double_click_time, double_click_distance, cursor_blink_time;
  GtkFontRendering font_rendering;
  char *font_name;
  int font_antialias, font_hinting;
  char *font_hinting_style, *subpixel_layout;
  int font_dpi;
  gboolean dark_theme;
  g_object_get(gtk_settings,
               "gtk-double-click-time", &double_click_time,
               "gtk-double-click-distance", &double_click_distance,
               "gtk-cursor-blink-time", &cursor_blink_time,
               "gtk-font-name", &font_name,
               "gtk-font-rendering", &font_rendering,
               "gtk-xft-antialias", &font_antialias,
               "gtk-xft-hinting", &font_hinting,
               "gtk-xft-hintstyle", &font_hinting_style,
               "gtk-xft-rgba", &subpixel_layout,
               "gtk-xft-dpi", &font_dpi,
               "gtk-application-prefer-dark-theme", &dark_theme,
               NULL);

  wpe_settings_set_value(settings, WPE_SETTING_DOUBLE_CLICK_TIME, g_variant_new_uint32(double_click_time), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
  wpe_settings_set_value(settings, WPE_SETTING_DOUBLE_CLICK_DISTANCE, g_variant_new_uint32(double_click_distance), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
  wpe_settings_set_value(settings, WPE_SETTING_CURSOR_BLINK_TIME, g_variant_new_uint32(cursor_blink_time), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
  wpe_settings_set_value(settings, WPE_SETTING_FONT_NAME, g_variant_new_string(font_name), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
  if (font_rendering == GTK_FONT_RENDERING_MANUAL) {
    wpe_settings_set_value(settings, WPE_SETTING_FONT_ANTIALIAS, g_variant_new_boolean(font_antialias != 0), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
    wpe_settings_set_value(settings, WPE_SETTING_FONT_HINTING_STYLE, g_variant_new_byte(font_hinting != 0 ? wpe_font_hinting_style(font_hinting_style) : WPE_SETTINGS_HINTING_STYLE_NONE), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
    wpe_settings_set_value(settings, WPE_SETTING_FONT_SUBPIXEL_LAYOUT, g_variant_new_byte(wpe_subpixel_layout(subpixel_layout)), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
  }
  wpe_settings_set_value(settings, WPE_SETTING_FONT_DPI, g_variant_new_double(font_dpi), WPE_SETTINGS_SOURCE_PLATFORM, NULL);
  wpe_settings_set_value(settings, WPE_SETTING_DARK_MODE, g_variant_new_boolean(dark_theme), WPE_SETTINGS_SOURCE_PLATFORM, NULL);

  g_free(font_name);
  g_free(font_hinting_style);
  g_free(subpixel_layout);
}

static void wpe_display_gtk_setup_settings(WPEDisplayGtk *display_gtk)
{
  GtkSettings *gtk_settings = gtk_settings_get_default();
  g_signal_connect_swapped(gtk_settings, "notify::gtk-double-click-time", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-double-click-distance", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-cursor-blink-time", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-font-name", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-xft-antialias", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-xft-hinting", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-xft-hintstyle", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-xft-rgba", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-xft-dpi", G_CALLBACK(gtk_settings_changed), display_gtk);
  g_signal_connect_swapped(gtk_settings, "notify::gtk-application-prefer-dark-theme", G_CALLBACK(gtk_settings_changed), display_gtk);

  gtk_settings_changed(display_gtk);
}

static gboolean wpe_display_gtk_connect(WPEDisplay *display, GError **error)
{
  WPEDisplayGtk *display_gtk = WPE_DISPLAY_GTK(display);
  if (!gtk_init_check()) {
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to initialize GTK");
    return FALSE;
  }

  display_gtk->display = gdk_display_get_default();
  if (!display_gtk->display) {
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to connect to default GDK display");
    return FALSE;
  }

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY(display_gtk->display))
    display_gtk->egl_display = gdk_wayland_display_get_egl_display(display_gtk->display);
#endif

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY(display_gtk->display))
    display_gtk->egl_display = gdk_x11_display_get_egl_display(display_gtk->display);
#endif

  if (display_gtk->egl_display == EGL_NO_DISPLAY) {
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to get GTK EGL display");
    display_gtk->display = NULL;
    return FALSE;
  }

  EGLDeviceEXT egl_device;
  if (eglQueryDisplayAttribEXT(display_gtk->egl_display, EGL_DEVICE_EXT, (EGLAttrib*)&egl_device)) {
    const char *extensions = eglQueryDeviceStringEXT(egl_device, EGL_EXTENSIONS);
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm"))
      display_gtk->drm_device = g_strdup(eglQueryDeviceStringEXT(egl_device, EGL_DRM_DEVICE_FILE_EXT));
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm_render_node"))
      display_gtk->drm_render_node = g_strdup(eglQueryDeviceStringEXT(egl_device, EGL_DRM_RENDER_NODE_FILE_EXT));
  }

  wpe_display_gtk_setup_screens(display_gtk);
  wpe_display_gtk_setup_dark_mode(display_gtk);
  wpe_display_gtk_setup_settings(display_gtk);

  return TRUE;
}

static gpointer wpe_display_gtk_get_egl_display(WPEDisplay *display, GError **error)
{
  return WPE_DISPLAY_GTK(display)->egl_display;
}

static WPEView *wpe_display_gtk_create_view(WPEDisplay *display)
{
  WPEDisplayGtk *display_gtk = WPE_DISPLAY_GTK(display);
  if (!display_gtk->display)
    return NULL;

  WPEViewGtk *view = WPE_VIEW_GTK(wpe_view_gtk_new(display_gtk));
  /* FIXME: create the toplevel conditionally. */
  GtkWindow *win = GTK_WINDOW(gtk_window_new());
  gtk_window_set_default_size(win, 1024, 768);
  gtk_window_set_child(win, wpe_view_gtk_get_widget(view));
  gtk_window_present(win);

  return WPE_VIEW(view);
}

static WPEKeymap *wpe_display_gtk_get_keymap(WPEDisplay *display, GError **error)
{
  WPEDisplayGtk *display_gtk = WPE_DISPLAY_GTK(display);
  if (!display_gtk->display)
    return NULL;

  if (!display_gtk->keymap)
    display_gtk->keymap = wpe_keymap_gtk_new(display_gtk->display);

  return display_gtk->keymap;
}

static WPEBufferDMABufFormats *wpe_display_gtk_get_preferred_dma_buf_formats(WPEDisplay *display)
{
  WPEDisplayGtk *display_gtk = WPE_DISPLAY_GTK(display);
  if (!display_gtk->display)
    return NULL;

  GdkDmabufFormats *formats = gdk_display_get_dmabuf_formats(display_gtk->display);
  gsize n_formats = gdk_dmabuf_formats_get_n_formats(formats);
  if (!n_formats)
    return NULL;

  WPEBufferDMABufFormatsBuilder *builder = wpe_buffer_dma_buf_formats_builder_new(NULL);
  wpe_buffer_dma_buf_formats_builder_append_group(builder, NULL, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING);
  for (gsize i = 0; i < n_formats; i++) {
    guint32 fourcc;
    guint64 modifier;
    gdk_dmabuf_formats_get_format(formats, i, &fourcc, &modifier);
    wpe_buffer_dma_buf_formats_builder_append_format(builder, fourcc, modifier);
  }

  return wpe_buffer_dma_buf_formats_builder_end(builder);
}

const char *wpe_display_gtk_get_drm_device(WPEDisplay *display)
{
  return WPE_DISPLAY_GTK(display)->drm_device;
}

const char *wpe_display_gtk_get_drm_render_node(WPEDisplay *display)
{
  return WPE_DISPLAY_GTK(display)->drm_render_node;
}

static guint wpe_display_gtk_get_n_screens(WPEDisplay *display)
{
  WPEDisplayGtk *display_gtk = WPE_DISPLAY_GTK(display);
  return display_gtk->screens ? display_gtk->screens->len : 0;
}

static WPEScreen *wpe_display_gtk_get_screen(WPEDisplay *display, guint index)
{
  WPEDisplayGtk *display_gtk = WPE_DISPLAY_GTK(display);
  if (!display_gtk->screens)
    return NULL;

  return index < display_gtk->screens->len ? g_ptr_array_index(display_gtk->screens, index) : NULL;
}


static void wpe_display_gtk_class_init(WPEDisplayGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = wpe_display_gtk_finalize;

  WPEDisplayClass *display_class = WPE_DISPLAY_CLASS(klass);
  display_class->connect = wpe_display_gtk_connect;
  display_class->get_egl_display = wpe_display_gtk_get_egl_display;
  display_class->create_view = wpe_display_gtk_create_view;
  display_class->get_keymap = wpe_display_gtk_get_keymap;
  display_class->get_preferred_dma_buf_formats = wpe_display_gtk_get_preferred_dma_buf_formats;
  display_class->get_drm_device = wpe_display_gtk_get_drm_device;
  display_class->get_drm_render_node = wpe_display_gtk_get_drm_render_node;
  display_class->get_n_screens = wpe_display_gtk_get_n_screens;
  display_class->get_screen = wpe_display_gtk_get_screen;
}

static void wpe_display_gtk_class_finalize(WPEDisplayGtkClass *klass)
{
}

static void wpe_display_gtk_init(WPEDisplayGtk *display)
{
  display->egl_display = EGL_NO_DISPLAY;
}

GdkDisplay *wpe_display_gtk_get_gdk_display(WPEDisplayGtk *display)
{
  g_return_val_if_fail(WPE_IS_DISPLAY_GTK(display), NULL);

  return display->display;
}

void wpe_display_gtk_register(GIOModule *module)
{
  wpe_display_gtk_register_type(G_TYPE_MODULE(module));
  if (!module)
    g_io_extension_point_register(WPE_DISPLAY_EXTENSION_POINT_NAME);
  g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, WPE_TYPE_DISPLAY_GTK, "wpe-display-gtk", 200);
}
