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

#include "wg-window.h"

#include "wg-tab-view.h"

struct _WGWindow {
  AdwApplicationWindow parent;

  GtkWidget *toolbar_view;
  GtkWidget *header_bar;
  GtkWidget *back_button;
  GtkWidget *forward_button;
  GtkWidget *new_tab_button;
  GtkWidget *url_entry;
  AdwTabBar *tab_bar;
  AdwTabView *tab_view;
  WebKitWebView *current_web_view;
  guint progress_timeout_id;
};

G_DEFINE_FINAL_TYPE(WGWindow, wg_window, ADW_TYPE_APPLICATION_WINDOW)

static void wg_window_go_back(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  WGWindow *win = WG_WINDOW(user_data);
  if (!win->current_web_view)
    return;

  webkit_web_view_go_back(win->current_web_view);
}

static void wg_window_go_forward(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  WGWindow *win = WG_WINDOW(user_data);
  if (!win->current_web_view)
    return;

  webkit_web_view_go_forward(win->current_web_view);
}

static AdwTabPage *wg_window_get_tab_page_for_web_view(WGWindow *win, WebKitWebView *web_view)
{
  guint n_pages = adw_tab_view_get_n_pages(win->tab_view);
  guint i;
  for (i = 0; i < n_pages; i++) {
    AdwTabPage *tab_page = adw_tab_view_get_nth_page(win->tab_view, i);
    WGTabView *tab_view = WG_TAB_VIEW(adw_tab_page_get_child(tab_page));

    if (wg_tab_view_get_web_view(tab_view) == web_view)
      return tab_page;
  }

  return NULL;
}

static void wg_window_close_tab(WGWindow *win, WebKitWebView *web_view)
{
  if (adw_tab_view_get_n_pages(win->tab_view) == 1) {
    gtk_window_destroy(GTK_WINDOW(win));
    return;
  }

  AdwTabPage *tab_page = wg_window_get_tab_page_for_web_view(win, web_view);
  if (!tab_page)
    return;

  adw_tab_view_close_page(win->tab_view, tab_page);
}

static AdwTabPage *wg_window_add_tab_page_for_view(WGWindow *win, WebKitWebView *web_view)
{
  GtkWidget *tab_view = wg_tab_view_new(web_view);
  AdwTabPage *tab_page = adw_tab_view_append(win->tab_view, tab_view);
  g_object_bind_property(G_OBJECT(web_view), "title", tab_page, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(web_view), "is-loading", tab_page, "loading", G_BINDING_SYNC_CREATE);

  g_signal_connect_object(web_view, "close", G_CALLBACK(wg_window_close_tab), win, G_CONNECT_SWAPPED);

  return tab_page;
}

static WebKitWebView *wg_window_create_web_view_for_new_tab(WGWindow *win)
{
  return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
    "display", wpe_view_get_display(webkit_web_view_get_wpe_view(win->current_web_view)),
    "web-context", webkit_web_view_get_context(win->current_web_view),
    "network-session", webkit_web_view_get_network_session(win->current_web_view),
    "settings", webkit_web_view_get_settings(win->current_web_view),
    NULL));
}

static void wg_window_new_tab(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  WGWindow *win = WG_WINDOW(user_data);
  if (!win->current_web_view)
    return;

  WebKitWebView *web_view = wg_window_create_web_view_for_new_tab(win);
  AdwTabPage *tab_page = wg_window_add_tab_page_for_view(win, web_view);
  adw_tab_view_set_selected_page(win->tab_view, tab_page);
  gtk_widget_grab_focus(win->url_entry);
}

static const GActionEntry actions[] = {
  { "go-back", wg_window_go_back },
  { "go-forward", wg_window_go_forward },
  { "new-tab", wg_window_new_tab },
};

static void wg_window_update_url(WGWindow *win)
{
  const char *url = win->current_web_view ? webkit_web_view_get_uri(win->current_web_view) : NULL;
  gtk_editable_set_text(GTK_EDITABLE(win->url_entry), url ? url : "");
}

static void wg_window_clear_load_progress(WGWindow *win)
{
  gtk_entry_set_progress_fraction(GTK_ENTRY(win->url_entry), 0);
  g_clear_handle_id(&win->progress_timeout_id, g_source_remove);
}

static void wg_window_update_load_progress(WGWindow *win)
{
  gdouble progress = win->current_web_view ? webkit_web_view_get_estimated_load_progress(win->current_web_view) : 0;
  gtk_entry_set_progress_fraction(GTK_ENTRY(win->url_entry), progress);
  g_clear_handle_id(&win->progress_timeout_id, g_source_remove);
  if (progress == 1.0)
    win->progress_timeout_id = g_timeout_add_once(500, (GSourceOnceFunc)wg_window_clear_load_progress, win);
}

static void wg_window_load_url(WGWindow *win)
{
  if (!win->current_web_view)
    return;

  webkit_web_view_load_uri(win->current_web_view, gtk_editable_get_text(GTK_EDITABLE(win->url_entry)));
  wg_tab_view_grab_focus(WG_TAB_VIEW(adw_tab_page_get_child(adw_tab_view_get_selected_page(win->tab_view))));
}

static void wg_window_update_navigation_actions(WGWindow *win)
{
  GAction *action = g_action_map_lookup_action(G_ACTION_MAP(win), "go-back");
  g_simple_action_set_enabled(G_SIMPLE_ACTION(action), win->current_web_view ? webkit_web_view_can_go_back(win->current_web_view) : FALSE);
  action = g_action_map_lookup_action(G_ACTION_MAP(win), "go-forward");
  g_simple_action_set_enabled(G_SIMPLE_ACTION(action), win->current_web_view ? webkit_web_view_can_go_forward(win->current_web_view) : FALSE);
}

static gboolean wg_window_decide_policy(WGWindow *win, WebKitPolicyDecision *decision, WebKitPolicyDecisionType decision_type)
{
  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    return FALSE;

  WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
  if (webkit_navigation_action_get_navigation_type(action) != WEBKIT_NAVIGATION_TYPE_LINK_CLICKED ||
      webkit_navigation_action_get_mouse_button(action) != WPE_BUTTON_MIDDLE)
    return FALSE;

  WebKitWebView *web_view = wg_window_create_web_view_for_new_tab(win);
  wg_window_add_tab_page_for_view(win, web_view);
  webkit_web_view_load_request(web_view, webkit_navigation_action_get_request(action));

  webkit_policy_decision_ignore(decision);
  return TRUE;
}

static WebKitWebView *wg_window_web_view_create(WGWindow *win, WebKitNavigationAction *navigation)
{
  WebKitWebView *web_view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
    "related-view", win->current_web_view,
    "settings", webkit_web_view_get_settings(win->current_web_view),
    NULL));
  AdwTabPage *tab_page = wg_window_add_tab_page_for_view(win, web_view);
  adw_tab_view_set_selected_page(win->tab_view, tab_page);
  return web_view;
}

static void wg_window_selected_page_changed(AdwTabView *tab_view_adw, GParamSpec *pspec, WGWindow *win)
{
  if (win->current_web_view) {
    g_signal_handlers_disconnect_by_data(win->current_web_view, win);

    WebKitBackForwardList *backForwardlist = webkit_web_view_get_back_forward_list(win->current_web_view);
    g_signal_handlers_disconnect_by_data(backForwardlist, win);

    g_object_unref(win->current_web_view);
  }

  AdwTabPage *tab_page = adw_tab_view_get_selected_page(tab_view_adw);
  win->current_web_view = tab_page ? wg_tab_view_get_web_view(WG_TAB_VIEW(adw_tab_page_get_child(tab_page))) : NULL;

  wg_window_update_url(win);
  wg_window_update_navigation_actions(win);
  if (!win->current_web_view || webkit_web_view_is_loading(win->current_web_view))
    wg_window_update_load_progress(win);

  if (win->current_web_view) {
    g_object_ref(win->current_web_view);

    g_signal_connect_object(win->current_web_view, "notify::uri", G_CALLBACK(wg_window_update_url), win, G_CONNECT_SWAPPED);
    g_signal_connect_object(win->current_web_view, "notify::estimated-load-progress", G_CALLBACK(wg_window_update_load_progress), win, G_CONNECT_SWAPPED);
    g_signal_connect_object(win->current_web_view, "decide-policy", G_CALLBACK(wg_window_decide_policy), win, G_CONNECT_SWAPPED);
    g_signal_connect_object(win->current_web_view, "create", G_CALLBACK(wg_window_web_view_create), win, G_CONNECT_SWAPPED);

    WebKitBackForwardList *backForwardlist = webkit_web_view_get_back_forward_list(win->current_web_view);
    g_signal_connect_object(backForwardlist, "changed", G_CALLBACK(wg_window_update_navigation_actions), win, G_CONNECT_SWAPPED);
  }
}

static void wg_window_constructed(GObject *object)
{
  G_OBJECT_CLASS(wg_window_parent_class)->constructed(object);

  WGWindow *win = WG_WINDOW(object);
  g_action_map_add_action_entries(G_ACTION_MAP(win), actions, G_N_ELEMENTS(actions), win);

  win->toolbar_view = adw_toolbar_view_new();

  win->header_bar = adw_header_bar_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(win->toolbar_view), win->header_bar);

  GtkWidget *start_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_add_css_class(box, "navigation-box");
  win->back_button = gtk_button_new_from_icon_name("go-previous-symbolic");
  gtk_actionable_set_action_name(GTK_ACTIONABLE(win->back_button), "win.go-back");
  gtk_widget_add_css_class(win->back_button, "toolbar-button");
  gtk_box_append(GTK_BOX(box), win->back_button);
  win->forward_button = gtk_button_new_from_icon_name("go-next-symbolic");
  gtk_actionable_set_action_name(GTK_ACTIONABLE(win->forward_button), "win.go-forward");
  gtk_widget_add_css_class(win->forward_button, "toolbar-button");
  gtk_box_append(GTK_BOX(box), win->forward_button);
  gtk_box_append(GTK_BOX(start_box), box);

  win->new_tab_button = gtk_button_new_from_icon_name("tab-new-symbolic");
  gtk_actionable_set_action_name(GTK_ACTIONABLE(win->new_tab_button), "win.new-tab");
  gtk_widget_add_css_class(win->new_tab_button, "toolbar-button");
  gtk_box_append(GTK_BOX(start_box), win->new_tab_button);

  adw_header_bar_pack_start(ADW_HEADER_BAR(win->header_bar), start_box);

  win->url_entry = gtk_entry_new();
  g_signal_connect_object(win->url_entry, "activate", G_CALLBACK(wg_window_load_url), win, G_CONNECT_SWAPPED);
  GtkWidget *clamp = adw_clamp_new();
  gtk_widget_set_hexpand(clamp, TRUE);
  adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 860);
  adw_clamp_set_tightening_threshold(ADW_CLAMP(clamp), 560);
  adw_clamp_set_child(ADW_CLAMP(clamp), win->url_entry);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(win->header_bar), clamp);

  win->tab_bar = adw_tab_bar_new();
  adw_tab_bar_set_autohide(win->tab_bar, TRUE);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(win->toolbar_view), GTK_WIDGET(win->tab_bar));

  win->tab_view = adw_tab_view_new();
  g_signal_connect(win->tab_view, "notify::selected-page", G_CALLBACK(wg_window_selected_page_changed), win);
  adw_tab_bar_set_view(win->tab_bar, win->tab_view);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(win->toolbar_view), GTK_WIDGET(win->tab_view));

  adw_application_window_set_content(ADW_APPLICATION_WINDOW(win), win->toolbar_view);
}

static void wg_window_dispose(GObject *object)
{
  WGWindow *win = WG_WINDOW(object);
  g_clear_handle_id(&win->progress_timeout_id, g_source_remove);

  G_OBJECT_CLASS(wg_window_parent_class)->dispose(object);
}

static void wg_window_finalize(GObject *object)
{
  WGWindow *win = WG_WINDOW(object);
  g_clear_object(&win->current_web_view);

  G_OBJECT_CLASS(wg_window_parent_class)->finalize(object);
}

static void wg_window_init(WGWindow *win)
{
}

static void wg_window_class_init(WGWindowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->constructed = wg_window_constructed;
  gobject_class->dispose = wg_window_dispose;
  gobject_class->finalize = wg_window_finalize;
}

GtkWidget *wg_window_new(void)
{
  return GTK_WIDGET(g_object_new(WG_TYPE_WINDOW, NULL));
}

void wg_window_add_web_view(WGWindow *win, WebKitWebView *web_view)
{
  g_return_if_fail(WG_IS_WINDOW(win));
  g_return_if_fail(WEBKIT_IS_WEB_VIEW(web_view));

  wg_window_add_tab_page_for_view(win, web_view);
}
