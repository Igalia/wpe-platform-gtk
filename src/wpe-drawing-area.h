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

#pragma once

#include <gtk/gtk.h>
#include <wpe/wpe-platform.h>

G_BEGIN_DECLS

#define WPE_TYPE_DRAWING_AREA (wpe_drawing_area_get_type())
G_DECLARE_FINAL_TYPE(WPEDrawingArea, wpe_drawing_area, WPE, DRAWING_AREA, GtkWidget)

GtkWidget  *wpe_drawing_area_new                 (WPEView        *view);
GtkWindow  *wpe_drawing_area_get_toplevel        (WPEDrawingArea *area);
GdkMonitor *wpe_drawing_area_get_current_monitor (WPEDrawingArea *area);
gboolean    wpe_drawing_area_render_buffer       (WPEDrawingArea *area,
                                                  WPEBuffer      *buffer,
                                                  GError        **error);

G_END_DECLS
