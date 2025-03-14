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

#include "wpe-drawing-area.h"

#include "wpe-display-gtk.h"
#include "wpe-toplevel-gtk.h"

#ifdef GTK_ACCESSIBILITY_ATSPI
#include <gtk/a11y/gtkatspi.h>
#endif

enum {
  PROP_0,

  PROP_VIEW,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct {
  double x;
  double y;
} MotionEvent;

struct _WPEDrawingArea {
  GtkWidget parent;

  WPEView *view;
  WPEBuffer *pending_buffer;
  WPEBuffer *committed_buffer;

  MotionEvent last_motion_event;

#ifdef GTK_ACCESSIBILITY_ATSPI
  GtkAccessible *accessible;
#endif
};

#ifdef GTK_ACCESSIBILITY_ATSPI
static void wpe_drawing_area_accessible_interface_init(GtkAccessibleInterface *iface);
static void wpe_drawing_area_view_accessible_interface_init(WPEViewAccessibleInterface* iface);

G_DEFINE_FINAL_TYPE_WITH_CODE(WPEDrawingArea, wpe_drawing_area, GTK_TYPE_WIDGET,
                              G_IMPLEMENT_INTERFACE(GTK_TYPE_ACCESSIBLE, wpe_drawing_area_accessible_interface_init)
                              G_IMPLEMENT_INTERFACE(WPE_TYPE_VIEW_ACCESSIBLE, wpe_drawing_area_view_accessible_interface_init))
#else
G_DEFINE_FINAL_TYPE(WPEDrawingArea, wpe_drawing_area, GTK_TYPE_WIDGET)
#endif

typedef struct {
  GdkDmabufTextureBuilder *builder;
  GdkTexture *texture;
} WPEBufferGtk;

static WPEBufferGtk *wpe_buffer_gtk_create(WPEBuffer *buffer)
{
  WPEBufferGtk *buffer_gtk = (WPEBufferGtk *)g_new0(WPEBufferGtk, 1);

  if (WPE_IS_BUFFER_DMA_BUF(buffer)) {
    WPEView *view = wpe_buffer_get_view(WPE_BUFFER(buffer));
    WPEDisplayGtk *display = WPE_DISPLAY_GTK(wpe_view_get_display(view));
    WPEBufferDMABuf *buffer_dmabuf = WPE_BUFFER_DMA_BUF(buffer);

    GdkDmabufTextureBuilder *builder = gdk_dmabuf_texture_builder_new();
    gdk_dmabuf_texture_builder_set_display(builder, wpe_display_gtk_get_gdk_display(display));
    gdk_dmabuf_texture_builder_set_width(builder, wpe_buffer_get_width(buffer));
    gdk_dmabuf_texture_builder_set_height(builder, wpe_buffer_get_height(buffer));
    gdk_dmabuf_texture_builder_set_fourcc(builder, wpe_buffer_dma_buf_get_format(buffer_dmabuf));
    gdk_dmabuf_texture_builder_set_modifier(builder, wpe_buffer_dma_buf_get_modifier(buffer_dmabuf));
    guint32 n_planes = wpe_buffer_dma_buf_get_n_planes(buffer_dmabuf);
    gdk_dmabuf_texture_builder_set_n_planes(builder, n_planes);
    for (guint32 i = 0; i < n_planes; i++) {
      gdk_dmabuf_texture_builder_set_fd(builder, i, wpe_buffer_dma_buf_get_fd(buffer_dmabuf, i));
      gdk_dmabuf_texture_builder_set_stride(builder, i, wpe_buffer_dma_buf_get_stride(buffer_dmabuf, i));
      gdk_dmabuf_texture_builder_set_offset(builder, i, wpe_buffer_dma_buf_get_offset(buffer_dmabuf, i));
    }

    buffer_gtk->builder = builder;
    return buffer_gtk;
  }

  if (WPE_IS_BUFFER_SHM(buffer))
    return buffer_gtk;

  g_free(buffer_gtk);
  return NULL;
}

static void wpe_buffer_gtk_free(WPEBufferGtk *buffer_gtk)
{
  g_clear_object(&buffer_gtk->builder);
  g_clear_object(&buffer_gtk->texture);

  g_free(buffer_gtk);
}

static void wpe_drawing_area_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  WPEDrawingArea *area = WPE_DRAWING_AREA(object);
  switch (prop_id) {
  case PROP_VIEW:
    area->view = g_value_dup_object(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void wpe_drawing_area_dispose(GObject *object)
{
  WPEDrawingArea *area = WPE_DRAWING_AREA(object);

  wpe_view_closed(area->view);
  g_clear_object(&area->view);
  g_clear_object(&area->pending_buffer);
  g_clear_object(&area->committed_buffer);

#ifdef GTK_ACCESSIBILITY_ATSPI
  g_clear_object(&area->accessible);
#endif

  G_OBJECT_CLASS(wpe_drawing_area_parent_class)->dispose(object);
}

static void wpe_drawing_area_size_allocate(GtkWidget *widget, int width, int height, int baseline)
{
  GTK_WIDGET_CLASS(wpe_drawing_area_parent_class)->size_allocate(widget, width, height, baseline);

  WPEDrawingArea *area = WPE_DRAWING_AREA(widget);
  wpe_view_resized(area->view, width, height);
}

static void wpe_drawing_area_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
  WPEDrawingArea *area = WPE_DRAWING_AREA(widget);

  gboolean notify_buffer_rendered = FALSE;
  if (area->pending_buffer) {
    notify_buffer_rendered = TRUE;
    if (area->committed_buffer)
      wpe_view_buffer_released(area->view, area->committed_buffer);
    g_set_object(&area->committed_buffer, g_steal_pointer(&area->pending_buffer));
  }

  if (!area->committed_buffer)
    return;

  WPEBufferGtk *buffer_gtk = wpe_buffer_get_user_data(area->committed_buffer);
  if (buffer_gtk && buffer_gtk->texture) {
    graphene_rect_t rect = GRAPHENE_RECT_INIT(0, 0, wpe_view_get_width(area->view), wpe_view_get_height(area->view));
    gtk_snapshot_append_texture(snapshot, buffer_gtk->texture, &rect);
  }

  if (notify_buffer_rendered)
    wpe_view_buffer_rendered(area->view, area->committed_buffer);
}

static WPEToplevel *get_or_create_toplevel_for_window(WPEDisplayGtk *display, GtkWindow *window)
{
  WPEToplevel *toplevel = g_object_get_data(G_OBJECT(window), "wpe-toplevel");
  if (!toplevel) {
    toplevel = wpe_toplevel_gtk_new(display, window);
    g_object_set_data_full(G_OBJECT(window), "wpe-toplevel", toplevel, g_object_unref);
  }
  return toplevel;
}

static void wpe_drawing_area_root(GtkWidget *widget)
{
  WPEDrawingArea *area = WPE_DRAWING_AREA(widget);

  GTK_WIDGET_CLASS(wpe_drawing_area_parent_class)->root(widget);

  GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(widget));
  WPEToplevel *toplevel = get_or_create_toplevel_for_window(WPE_DISPLAY_GTK(wpe_view_get_display(area->view)), window);
  wpe_view_set_toplevel(area->view, toplevel);
}

static void wpe_drawing_area_unroot(GtkWidget *widget)
{
  WPEDrawingArea *area = WPE_DRAWING_AREA(widget);

  GTK_WIDGET_CLASS(wpe_drawing_area_parent_class)->unroot(widget);

  wpe_view_set_toplevel(area->view, NULL);
}

static void wpe_drawing_area_map(GtkWidget *widget)
{
  GTK_WIDGET_CLASS(wpe_drawing_area_parent_class)->map(widget);

  WPEDrawingArea *area = WPE_DRAWING_AREA(widget);
  wpe_view_map(area->view);
}

static void wpe_drawing_area_unmap(GtkWidget *widget)
{
  GTK_WIDGET_CLASS(wpe_drawing_area_parent_class)->unmap(widget);

  WPEDrawingArea *area = WPE_DRAWING_AREA(widget);
  wpe_view_unmap(area->view);
}

static void wpe_drawing_area_class_init(WPEDrawingAreaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = wpe_drawing_area_set_property;
  object_class->dispose = wpe_drawing_area_dispose;

  properties[PROP_VIEW] =
    g_param_spec_object("view",
                        NULL, NULL,
                        WPE_TYPE_VIEW,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties(object_class, N_PROPS, properties);

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->size_allocate = wpe_drawing_area_size_allocate;
  widget_class->snapshot = wpe_drawing_area_snapshot;
  widget_class->root = wpe_drawing_area_root;
  widget_class->unroot = wpe_drawing_area_unroot;
  widget_class->map = wpe_drawing_area_map;
  widget_class->unmap = wpe_drawing_area_unmap;
}

static WPEModifiers wpe_modifiers_for_gdk_modifiers(GdkModifierType modifiers)
{
  WPEModifiers retval = 0;
  if (modifiers & GDK_CONTROL_MASK)
    retval |= WPE_MODIFIER_KEYBOARD_CONTROL;
  if (modifiers & GDK_SHIFT_MASK)
    retval |= WPE_MODIFIER_KEYBOARD_SHIFT;
  if (modifiers & GDK_ALT_MASK)
    retval |= WPE_MODIFIER_KEYBOARD_ALT;
  if (modifiers & GDK_META_MASK)
    retval |= WPE_MODIFIER_KEYBOARD_META;
  if (modifiers & GDK_LOCK_MASK)
    retval |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
  if (modifiers & GDK_BUTTON1_MASK)
    retval |= WPE_MODIFIER_POINTER_BUTTON1;
  if (modifiers & GDK_BUTTON2_MASK)
    retval |= WPE_MODIFIER_POINTER_BUTTON2;
  if (modifiers & GDK_BUTTON3_MASK)
    retval |= WPE_MODIFIER_POINTER_BUTTON3;
  if (modifiers & GDK_BUTTON4_MASK)
    retval |= WPE_MODIFIER_POINTER_BUTTON4;
  if (modifiers & GDK_BUTTON5_MASK)
    retval |= WPE_MODIFIER_POINTER_BUTTON5;
  return retval;
}

WPEInputSource wpe_input_source_for_gdk_device(GdkDevice *device)
{
  switch (gdk_device_get_source(device)) {
  case GDK_SOURCE_MOUSE:
    return WPE_INPUT_SOURCE_MOUSE;
  case GDK_SOURCE_PEN:
    return WPE_INPUT_SOURCE_PEN;
  case GDK_SOURCE_KEYBOARD:
    return WPE_INPUT_SOURCE_KEYBOARD;
  case GDK_SOURCE_TOUCHSCREEN:
    return WPE_INPUT_SOURCE_TOUCHSCREEN;
  case GDK_SOURCE_TOUCHPAD:
    return WPE_INPUT_SOURCE_TOUCHPAD;
  case GDK_SOURCE_TRACKPOINT:
    return WPE_INPUT_SOURCE_TRACKPOINT;
  case GDK_SOURCE_TABLET_PAD:
    return WPE_INPUT_SOURCE_TABLET_PAD;
  }
  return WPE_INPUT_SOURCE_MOUSE;
}

static guint wpe_button_for_gdk_button(guint button)
{
  switch (button) {
  case GDK_BUTTON_PRIMARY:
    return WPE_BUTTON_PRIMARY;
  case GDK_BUTTON_MIDDLE:
    return WPE_BUTTON_MIDDLE;
  case GDK_BUTTON_SECONDARY:
    return WPE_BUTTON_SECONDARY;
  default:
    break;
  }
  return button;
}

static void wpe_drawing_area_focus_enter(WPEDrawingArea *area, GtkEventController *controller)
{
  wpe_view_focus_in(area->view);
}

static void wpe_drawing_area_focus_leave(WPEDrawingArea *area, GtkEventController *controller)
{
  wpe_view_focus_out(area->view);
}

static void wpe_drawing_area_pointer_enter(WPEDrawingArea *area, double x, double y, GdkCrossingMode mode, GtkEventController *controller)
{
  g_autoptr(WPEEvent) event =
    wpe_event_pointer_move_new(WPE_EVENT_POINTER_ENTER,
                               area->view,
                               WPE_INPUT_SOURCE_MOUSE,
                               0, 0, x, y, 0, 0);
  wpe_view_event(area->view, event);
}

static gboolean wpe_drawing_area_pointer_motion(WPEDrawingArea *area, double x, double y, GtkEventController *controller)
{
  GdkEvent *gdk_event = gtk_event_controller_get_current_event(controller);
  double delta_x = 0, delta_y = 0;
  if (area->last_motion_event.x != -1 && area->last_motion_event.y != -1) {
    delta_x = x - area->last_motion_event.x;
    delta_y = y - area->last_motion_event.y;
  }
  area->last_motion_event.x = x;
  area->last_motion_event.y = y;

  g_autoptr(WPEEvent) event =
    wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE,
                               area->view,
                               wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                               gdk_event_get_time(gdk_event),
                               wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                               x, y, delta_x, delta_y);
  wpe_view_event(area->view, event);

  return GDK_EVENT_PROPAGATE;
}

static void wpe_drawing_area_pointer_leave(WPEDrawingArea *area, GdkCrossingMode mode, GtkEventController *controller)
{
  g_autoptr(WPEEvent) event =
    wpe_event_pointer_move_new(WPE_EVENT_POINTER_LEAVE,
                               area->view,
                               WPE_INPUT_SOURCE_MOUSE,
                               0, 0, -1, -1, 0, 0);
  wpe_view_event(area->view, event);
}

static void wpe_drawing_area_scroll_begin(WPEDrawingArea *area, GtkEventController *controller)
{
  GdkEvent* gdk_event = gtk_event_controller_get_current_event(controller);
  g_autoptr(WPEEvent) event =
    wpe_event_scroll_new(area->view,
                         wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                         gdk_event_get_time(gdk_event),
                         wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                         0, 0,
                         gdk_event_get_event_type(gdk_event) != GDK_SCROLL || gdk_scroll_event_get_unit(gdk_event) != GDK_SCROLL_UNIT_WHEEL,
                         FALSE,
                         area->last_motion_event.x != -1 ? area->last_motion_event.x : 0,
                         area->last_motion_event.y != -1 ? area->last_motion_event.y : 0);
  wpe_view_event(area->view, event);
}

static gboolean wpe_drawing_area_scroll(WPEDrawingArea *area, double x, double y, GtkEventController *controller)
{
  GdkEvent* gdk_event = gtk_event_controller_get_current_event(controller);
  g_autoptr(WPEEvent) event =
    wpe_event_scroll_new(area->view,
                         wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                         gdk_event_get_time(gdk_event),
                         wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                         -x, -y,
                         gdk_event_get_event_type(gdk_event) != GDK_SCROLL || gdk_scroll_event_get_unit(gdk_event) != GDK_SCROLL_UNIT_WHEEL,
                         FALSE,
                         area->last_motion_event.x != -1 ? area->last_motion_event.x : 0,
                         area->last_motion_event.y != -1 ? area->last_motion_event.y : 0);
  wpe_view_event(area->view, event);
  return GDK_EVENT_STOP;
}

static void wpe_drawing_area_scroll_end(WPEDrawingArea *area, GtkEventController *controller)
{
  GdkEvent* gdk_event = gtk_event_controller_get_current_event(controller);
  if (!gdk_event)
    return;

  g_autoptr(WPEEvent) event =
    wpe_event_scroll_new(area->view,
                         wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                         gdk_event_get_time(gdk_event),
                         wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                         0, 0,
                         gdk_event_get_event_type(gdk_event) != GDK_SCROLL || gdk_scroll_event_get_unit(gdk_event) != GDK_SCROLL_UNIT_WHEEL,
                         TRUE,
                         area->last_motion_event.x != -1 ? area->last_motion_event.x : 0,
                         area->last_motion_event.y != -1 ? area->last_motion_event.y : 0);
  wpe_view_event(area->view, event);
}

static void wpe_drawing_area_button_pressed(WPEDrawingArea *area, int click_count, double x, double y, GtkGesture *gesture)
{
  if (gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture)))
    return;

  gtk_widget_grab_focus(GTK_WIDGET(area));
  gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);

  GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
  GdkEvent *gdk_event = gtk_gesture_get_last_event(gesture, sequence);
  g_autoptr(WPEEvent) event =
    wpe_event_pointer_button_new(WPE_EVENT_POINTER_DOWN,
                                 area->view,
                                 wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                                 gdk_event_get_time(gdk_event),
                                 wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                                 wpe_button_for_gdk_button(gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture))),
                                 x, y, click_count);
  wpe_view_event(area->view, event);
}

static void wpe_drawing_area_button_released(WPEDrawingArea *area, int click_count, double x, double y, GtkGesture *gesture)
{
  if (gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture)))
    return;

  gtk_widget_grab_focus(GTK_WIDGET(area));
  gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);

  GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
  GdkEvent *gdk_event = gtk_gesture_get_last_event(gesture, sequence);
  g_autoptr(WPEEvent) event =
    wpe_event_pointer_button_new(WPE_EVENT_POINTER_UP,
                                 area->view,
                                 wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                                 gdk_event_get_time(gdk_event),
                                 wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                                 wpe_button_for_gdk_button(gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture))),
                                 x, y, 0);
  wpe_view_event(area->view, event);
}

static gboolean wpe_drawing_area_key_pressed(WPEDrawingArea *area, guint keyval, guint keycode, GdkModifierType modifiers, GtkEventController *controller)
{
  GdkEvent* gdk_event = gtk_event_controller_get_current_event(controller);
  g_autoptr(WPEEvent) event =
    wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_DOWN,
                           area->view,
                           wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                           gdk_event_get_time(gdk_event),
                           wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                           keycode, keyval);
  wpe_event_set_user_data(event, gdk_event_ref(gdk_event), (GDestroyNotify)gdk_event_unref);
  wpe_view_event(area->view, event);
  return GDK_EVENT_STOP;
}

static gboolean wpe_drawing_area_key_released(WPEDrawingArea *area, guint keyval, guint keycode, GdkModifierType modifiers, GtkEventController *controller)
{
  GdkEvent* gdk_event = gtk_event_controller_get_current_event(controller);
  g_autoptr(WPEEvent) event =
    wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_UP,
                           area->view,
                           wpe_input_source_for_gdk_device(gdk_event_get_device(gdk_event)),
                           gdk_event_get_time(gdk_event),
                           wpe_modifiers_for_gdk_modifiers(gdk_event_get_modifier_state(gdk_event)),
                           keycode, keyval);
  wpe_event_set_user_data(event, gdk_event_ref(gdk_event), (GDestroyNotify)gdk_event_unref);
  wpe_view_event(area->view, event);
  return GDK_EVENT_STOP;
}

static void wpe_drawing_area_init(WPEDrawingArea *area)
{
  GtkWidget *widget = GTK_WIDGET(area);
  gtk_widget_set_focusable(widget, TRUE);
  gtk_widget_set_can_focus(widget, TRUE);

  area->last_motion_event.x = -1;
  area->last_motion_event.y = -1;

  GtkEventController *controller = gtk_event_controller_focus_new();
  g_signal_connect_object(controller, "enter", G_CALLBACK(wpe_drawing_area_focus_enter), widget, G_CONNECT_SWAPPED);
  g_signal_connect_object(controller, "leave", G_CALLBACK(wpe_drawing_area_focus_leave), widget, G_CONNECT_SWAPPED);
  gtk_widget_add_controller(widget, controller);

  controller = gtk_event_controller_motion_new();
  g_signal_connect_object(controller, "enter", G_CALLBACK(wpe_drawing_area_pointer_enter), widget, G_CONNECT_SWAPPED);
  g_signal_connect_object(controller, "motion", G_CALLBACK(wpe_drawing_area_pointer_motion), widget, G_CONNECT_SWAPPED);
  g_signal_connect_object(controller, "leave", G_CALLBACK(wpe_drawing_area_pointer_leave), widget, G_CONNECT_SWAPPED);
  gtk_widget_add_controller(widget, controller);

  controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  g_signal_connect_object(controller, "scroll-begin", G_CALLBACK(wpe_drawing_area_scroll_begin), widget, G_CONNECT_SWAPPED);
  g_signal_connect_object(controller, "scroll", G_CALLBACK(wpe_drawing_area_scroll), widget, G_CONNECT_SWAPPED);
  g_signal_connect_object(controller, "scroll-end", G_CALLBACK(wpe_drawing_area_scroll_end), widget, G_CONNECT_SWAPPED);
  gtk_widget_add_controller(widget, controller);

  controller = GTK_EVENT_CONTROLLER(gtk_gesture_click_new());
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(controller), 0);
  gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(controller), TRUE);
  g_signal_connect_object(controller, "pressed", G_CALLBACK(wpe_drawing_area_button_pressed), widget, G_CONNECT_SWAPPED);
  g_signal_connect_object(controller, "released", G_CALLBACK(wpe_drawing_area_button_released), widget, G_CONNECT_SWAPPED);
  gtk_widget_add_controller(widget, controller);

  controller = gtk_event_controller_key_new();
  g_signal_connect_object(controller, "key-pressed", G_CALLBACK(wpe_drawing_area_key_pressed), widget, G_CONNECT_SWAPPED);
  g_signal_connect_object(controller, "key-released", G_CALLBACK(wpe_drawing_area_key_released), widget, G_CONNECT_SWAPPED);
  gtk_widget_add_controller(widget, controller);
}

GtkWidget *wpe_drawing_area_new(WPEView *view)
{
  g_return_val_if_fail(WPE_IS_VIEW(view), NULL);

  return g_object_new(WPE_TYPE_DRAWING_AREA, "view", view, NULL);
}

static void wpe_drawing_area_update_buffer_damage(WPEDrawingArea *area, WPEBufferGtk *buffer_gtk, const WPERectangle *damage_rects, guint n_damage_rects)
{
  if (n_damage_rects > 0 && area->committed_buffer) {
    WPEBufferGtk *committed_buffer_gtk = wpe_buffer_get_user_data(area->committed_buffer);
    gdk_dmabuf_texture_builder_set_update_texture(buffer_gtk->builder, committed_buffer_gtk->texture);
    cairo_region_t *region = cairo_region_create();
    for (guint i = 0; i < n_damage_rects; i++) {
      cairo_rectangle_int_t rect = { damage_rects[i].x, damage_rects[i].y, damage_rects[i].width, damage_rects[i].height };
      cairo_region_union_rectangle(region, &rect);
    }
    gdk_dmabuf_texture_builder_set_update_region(buffer_gtk->builder, region);
    cairo_region_destroy(region);
  } else {
    gdk_dmabuf_texture_builder_set_update_texture(buffer_gtk->builder, NULL);
    gdk_dmabuf_texture_builder_set_update_region(buffer_gtk->builder, NULL);
  }
}

static gboolean wpe_drawing_area_ensure_texture(WPEDrawingArea *area, WPEBuffer *buffer, const WPERectangle *damage_rects, guint n_damage_rects, GError **error)
{
  WPEBufferGtk* buffer_gtk = wpe_buffer_get_user_data(buffer);
  if (!buffer_gtk) {
    buffer_gtk = wpe_buffer_gtk_create(buffer);
    if (!buffer_gtk) {
      g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: unsupported buffer");
      return FALSE;
    }
    wpe_buffer_set_user_data(buffer, buffer_gtk, (GDestroyNotify)wpe_buffer_gtk_free);
  }

  if (buffer_gtk->builder) {
    wpe_drawing_area_update_buffer_damage(area, buffer_gtk, damage_rects, n_damage_rects);
    g_autoptr(GError) buffer_error = NULL;
    g_set_object(&buffer_gtk->texture, gdk_dmabuf_texture_builder_build(buffer_gtk->builder, NULL, NULL, &buffer_error));
    if (!buffer_gtk->texture) {
      g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to build DMA-BUF texture: %s", buffer_error->message);
      return FALSE;
    }
  } else {
    g_set_object(&buffer_gtk->texture, gdk_memory_texture_new(wpe_buffer_get_width(buffer),
                                                              wpe_buffer_get_height(buffer),
                                                              GDK_MEMORY_DEFAULT,
                                                              wpe_buffer_shm_get_data(WPE_BUFFER_SHM(buffer)),
                                                              wpe_buffer_shm_get_stride(WPE_BUFFER_SHM(buffer))));
  }

  return TRUE;
}

gboolean wpe_drawing_area_render_buffer(WPEDrawingArea *area, WPEBuffer *buffer, const WPERectangle *damage_rects, guint n_damage_rects, GError **error)
{
  g_return_val_if_fail(WPE_IS_DRAWING_AREA(area), FALSE);

  if (!wpe_drawing_area_ensure_texture(area, buffer, damage_rects, n_damage_rects, error))
    return FALSE;

  g_set_object(&area->pending_buffer, buffer);
  gtk_widget_queue_draw(GTK_WIDGET(area));
  return TRUE;
}

#ifdef GTK_ACCESSIBILITY_ATSPI
static GtkAccessible *wpe_drawing_area_get_first_accessible_child(GtkAccessible* accessible)
{
  WPEDrawingArea *area = WPE_DRAWING_AREA(accessible);

  if (area->accessible)
    return GTK_ACCESSIBLE(g_object_ref(area->accessible));

  GtkWidget *widget = gtk_widget_get_first_child(GTK_WIDGET(area));
  if (widget)
    return GTK_ACCESSIBLE(g_object_ref(widget));

  return NULL;
}

static void wpe_drawing_area_accessible_interface_init(GtkAccessibleInterface* iface)
{
  iface->get_first_accessible_child = wpe_drawing_area_get_first_accessible_child;
}

static void wpe_drawing_area_bind(WPEViewAccessible *accessible, const char *plugID)
{
  WPEDrawingArea *area = WPE_DRAWING_AREA(accessible);

  if (area->accessible) {
    gtk_accessible_set_accessible_parent(area->accessible, NULL, NULL);
    g_clear_object(&area->accessible);
  }

  GError *error = NULL;
  char **tokens = g_strsplit(plugID, ":", 2);
  if (!g_dbus_is_unique_name(tokens[0])) {
    area->accessible = gtk_at_spi_socket_new(tokens[0], tokens[1], &error);
  } else {
    char *bus_name = g_strdup_printf(":%s", tokens[0]);

    area->accessible = gtk_at_spi_socket_new(bus_name, tokens[1], &error);
    g_free(bus_name);
  }
  g_strfreev(tokens);

  if (!area->accessible) {
    g_warning("Error creating WPEViewGtk accessibility socket: %s", error->message);
    g_error_free(error);
    return;
  }

  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(area));
  gtk_accessible_set_accessible_parent(area->accessible, GTK_ACCESSIBLE(area), GTK_ACCESSIBLE(child));
}

static void wpe_drawing_area_view_accessible_interface_init(WPEViewAccessibleInterface* iface)
{
  iface->bind = wpe_drawing_area_bind;
}
#endif /* GTK_ACCESSIBILITY_ATSPI */
