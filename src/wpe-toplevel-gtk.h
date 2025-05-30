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

#ifndef _WPE_TOPLEVEL_GTK_H_
#define _WPE_TOPLEVEL_GTK_H_

#include "wpe-display-gtk.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WPE_TYPE_TOPLEVEL_GTK (wpe_toplevel_gtk_get_type())
G_DECLARE_FINAL_TYPE(WPEToplevelGtk, wpe_toplevel_gtk, WPE, TOPLEVEL_GTK, WPEToplevel)

WPEToplevel *wpe_toplevel_gtk_new           (WPEDisplayGtk  *display,
                                             guint           max_views,
                                             GtkWindow      *window);
GtkWindow   *wpe_toplevel_gtk_get_window    (WPEToplevelGtk *toplevel);
gboolean     wpe_toplevel_gtk_is_in_screen  (WPEToplevelGtk *toplevel);

G_END_DECLS

#endif /* _WPE_TOPLEVEL_GTK_H_ */
