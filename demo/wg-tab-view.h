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

#ifndef _WG_TAB_VIEW_H_
#define _WG_TAB_VIEW_H_

#include <adwaita.h>
#include <gtk/gtk.h>
#include <wpe/webkit.h>

G_BEGIN_DECLS

#define WG_TYPE_TAB_VIEW (wg_tab_view_get_type())
G_DECLARE_FINAL_TYPE(WGTabView, wg_tab_view, WG, TAB_VIEW, AdwBin)

GtkWidget     *wg_tab_view_new          (WebKitWebView *web_view);
WebKitWebView *wg_tab_view_get_web_view (WGTabView     *tab_view);
void           wg_tab_view_grab_focus   (WGTabView     *tab_view);

G_END_DECLS

#endif /* _WG_TAB_VIEW_H_ */
