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

#include "wg-tab-view.h"

#include "wpe-view-gtk.h"

struct _WGTabView {
  AdwBin parent;

  WebKitWebView *web_view;
};

G_DEFINE_FINAL_TYPE(WGTabView, wg_tab_view, ADW_TYPE_BIN)

static void wg_tab_view_finalize(GObject *object)
{
  WGTabView *tab_view = WG_TAB_VIEW(object);
  g_clear_object(&tab_view->web_view);

  G_OBJECT_CLASS(wg_tab_view_parent_class)->finalize(object);
}

static void wg_tab_view_init(WGTabView *tab_view)
{
}

static void wg_tab_view_class_init(WGTabViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->finalize = wg_tab_view_finalize;
}

GtkWidget *wg_tab_view_new(WebKitWebView *web_view)
{
  g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(web_view), NULL);

  WGTabView *tab_view = WG_TAB_VIEW(g_object_new(WG_TYPE_TAB_VIEW, NULL));
  tab_view->web_view = g_object_ref(web_view);
  adw_bin_set_child(ADW_BIN(tab_view), wpe_view_gtk_get_widget(WPE_VIEW_GTK(webkit_web_view_get_wpe_view(web_view))));

  return GTK_WIDGET(tab_view);
}

WebKitWebView *wg_tab_view_get_web_view(WGTabView *tab_view)
{
  return tab_view->web_view;
}

void wg_tab_view_grab_focus(WGTabView *tab_view)
{
  gtk_widget_grab_focus(wpe_view_gtk_get_widget(WPE_VIEW_GTK(webkit_web_view_get_wpe_view(tab_view->web_view))));
}
