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

#include "wpe-keymap-gtk.h"

struct _WPEKeymapGtk {
  WPEKeymap parent;

  GdkDisplay *display;
};

G_DEFINE_FINAL_TYPE(WPEKeymapGtk, wpe_keymap_gtk, WPE_TYPE_KEYMAP)

static void wpe_keymap_gtk_finalize(GObject *object)
{
  WPEKeymapGtk *keymap_gtk = WPE_KEYMAP_GTK(object);
  g_clear_object(&keymap_gtk->display);

  G_OBJECT_CLASS(wpe_keymap_gtk_parent_class)->finalize(object);
}

static GdkModifierType wpe_modifiers_to_gdk_modifiers(WPEModifiers modifiers)
{
  GdkModifierType state = 0;
  if (modifiers & WPE_MODIFIER_KEYBOARD_SHIFT)
    state |= GDK_SHIFT_MASK;
  if (modifiers & WPE_MODIFIER_KEYBOARD_CONTROL)
    state |= GDK_CONTROL_MASK;
  if (modifiers & WPE_MODIFIER_KEYBOARD_ALT)
    state |= GDK_ALT_MASK;
  if (modifiers & WPE_MODIFIER_KEYBOARD_META)
    state |= GDK_META_MASK;
  return state;
}

static WPEModifiers wpe_modifiers_from_gdk_modifiers(GdkModifierType state)
{
  WPEModifiers modifiers = 0;
  if (state & GDK_SHIFT_MASK)
    modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
  if (state & GDK_CONTROL_MASK)
    modifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
  if (state & GDK_ALT_MASK)
    modifiers |= WPE_MODIFIER_KEYBOARD_ALT;
  if (state & GDK_META_MASK)
    modifiers |= WPE_MODIFIER_KEYBOARD_META;
  return modifiers;
}

static gboolean wpe_keymap_gtk_get_entries_for_keyval(WPEKeymap *keymap, guint keyval, WPEKeymapEntry **entries, guint *n_entries)
{
  WPEKeymapGtk *keymap_gtk = WPE_KEYMAP_GTK(keymap);
  GdkKeymapKey *gdk_entries = NULL;
  int gdk_n_entries;
  if (gdk_display_map_keyval(keymap_gtk->display, keyval, &gdk_entries, &gdk_n_entries)) {
    *entries = g_new(WPEKeymapEntry, gdk_n_entries);
    for (guint i = 0; i < *n_entries; i++) {
      (*entries[i]).keycode = gdk_entries[i].keycode;
      (*entries[i]).group = gdk_entries[i].group;
      (*entries[i]).level = gdk_entries[i].level;
    }
    g_free(gdk_entries);

    *n_entries = gdk_n_entries;

    return TRUE;
  }

  return FALSE;
}

static gboolean wpe_keymap_gtk_translate_keyboard_state(WPEKeymap *keymap, guint keycode, WPEModifiers modifiers, int group, guint *keyval, int *effective_group, int *level, WPEModifiers *consumed_modifiers)
{
  WPEKeymapGtk *keymap_gtk = WPE_KEYMAP_GTK(keymap);
  GdkModifierType gdk_consumed_modifiers;
  if (!gdk_display_translate_key(keymap_gtk->display, keycode, wpe_modifiers_to_gdk_modifiers(modifiers), group, keyval, effective_group, level, consumed_modifiers ? &gdk_consumed_modifiers : NULL))
    return FALSE;

  if (consumed_modifiers)
    *consumed_modifiers = wpe_modifiers_from_gdk_modifiers(gdk_consumed_modifiers);

  return TRUE;
}

static WPEModifiers wpe_keymap_gtk_get_modifiers(WPEKeymap *keymap)
{
  WPEKeymapGtk *keymap_gtk = WPE_KEYMAP_GTK(keymap);
  GdkDevice *device = gdk_seat_get_keyboard(gdk_display_get_default_seat(keymap_gtk->display));
  WPEModifiers modifiers = wpe_modifiers_from_gdk_modifiers(gdk_device_get_modifier_state(device));
  if (gdk_device_get_caps_lock_state(device))
    modifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;

  return modifiers;
}

static void wpe_keymap_gtk_class_init(WPEKeymapGtkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = wpe_keymap_gtk_finalize;

  WPEKeymapClass* keymap_class = WPE_KEYMAP_CLASS(klass);
  keymap_class->get_entries_for_keyval = wpe_keymap_gtk_get_entries_for_keyval;
  keymap_class->translate_keyboard_state = wpe_keymap_gtk_translate_keyboard_state;
  keymap_class->get_modifiers = wpe_keymap_gtk_get_modifiers;
}

static void wpe_keymap_gtk_init(WPEKeymapGtk *keymap_gtk)
{
}

WPEKeymap *wpe_keymap_gtk_new(GdkDisplay *display)
{
  g_return_val_if_fail(GDK_IS_DISPLAY(display), NULL);

  WPEKeymapGtk *keymap = WPE_KEYMAP_GTK(g_object_new(WPE_TYPE_KEYMAP_GTK, NULL));
  keymap->display = g_object_ref(display);
  return WPE_KEYMAP(keymap);
}
