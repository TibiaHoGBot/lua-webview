/*
 * MIT License
 *
 * Copyright (c) 2017 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

static void external_message_received_cb(WebKitUserContentManager *m,
                                         WebKitJavascriptResult *r,
                                         gpointer arg) {
  (void)m;
  struct webview *w = (struct webview *)arg;
  if (w->external_invoke_cb == NULL) {
    return;
  }
#if WEBKIT_MAJOR_VERSION >= 2 && WEBKIT_MINOR_VERSION >= 22
  JSCValue *value = webkit_javascript_result_get_js_value(r);
  char *s = jsc_value_to_string(value);
#else
  JSGlobalContextRef context = webkit_javascript_result_get_global_context(r);
  JSValueRef value = webkit_javascript_result_get_value(r);
  JSStringRef js = JSValueToStringCopy(context, value, NULL);
  size_t n = JSStringGetMaximumUTF8CStringSize(js);
  char *s = g_new(char, n);
  JSStringGetUTF8CString(js, s, n);
  JSStringRelease(js);
#endif
  w->external_invoke_cb(w, s);
  g_free(s);
}

#define REGISTER_EXTERNAL_INVOKE_JS \
  "window.external={invoke:function(x){" \
  "window.webkit.messageHandlers.external.postMessage(x);" \
  "}}"

static void webview_load_changed_cb(WebKitWebView *webview,
                                    WebKitLoadEvent event, gpointer arg) {
  (void)webview;
  struct webview *w = (struct webview *)arg;
  if (event == WEBKIT_LOAD_STARTED) {
    w->priv.ready = 0;
  } else if (event == WEBKIT_LOAD_FINISHED) {
    webkit_web_view_run_javascript(
      WEBKIT_WEB_VIEW(w->priv.webview),
      "if(!window.external){" REGISTER_EXTERNAL_INVOKE_JS "}",
      NULL, NULL, NULL);
    w->priv.ready = 1;
  }
}

static void webview_destroy_cb(GtkWidget *widget, gpointer arg) {
  (void)widget;
  struct webview *w = (struct webview *)arg;
  webview_terminate(w);
}

static gboolean webview_context_menu_cb(WebKitWebView *webview,
                                        GtkWidget *default_menu,
                                        WebKitHitTestResult *hit_test_result,
                                        gboolean triggered_with_keyboard,
                                        gpointer userdata) {
  (void)webview;
  (void)default_menu;
  (void)hit_test_result;
  (void)triggered_with_keyboard;
  (void)userdata;
  return TRUE;
}

static void noLogHandler(const gchar *domain, GLogLevelFlags level, const gchar *message, gpointer data) {
  return;
}

WEBVIEW_API int webview_init(struct webview *w) {

  if (!w->debug) {
    g_log_set_handler("GLib", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, noLogHandler, NULL);
  }
  //gtk_disable_setlocale();

  if (gtk_init_check(0, NULL) == FALSE) {
    return -1;
  }

  w->priv.ready = 0;
  w->priv.should_exit = 0;
  w->priv.queue = g_async_queue_new();
  w->priv.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(w->priv.window), w->title);

  if (w->resizable) {
    gtk_window_set_default_size(GTK_WINDOW(w->priv.window), w->width,
                                w->height);
  } else {
    gtk_widget_set_size_request(w->priv.window, w->width, w->height);
  }
  gtk_window_set_resizable(GTK_WINDOW(w->priv.window), !!w->resizable);
  gtk_window_set_position(GTK_WINDOW(w->priv.window), GTK_WIN_POS_CENTER);

  w->priv.scroller = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(w->priv.window), w->priv.scroller);

  WebKitUserContentManager *m = webkit_user_content_manager_new();
  webkit_user_content_manager_register_script_message_handler(m, "external");
  g_signal_connect(m, "script-message-received::external",
                   G_CALLBACK(external_message_received_cb), w);

  w->priv.webview = webkit_web_view_new_with_user_content_manager(m);

  if (getenv("WEBVIEW_GTK_NO_SPURIOUS_REF") == NULL) {
    g_object_ref(w->priv.webview); // work around core dump
  }

  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(w->priv.webview),
                           webview_check_url(w->url));
  g_signal_connect(G_OBJECT(w->priv.webview), "load-changed",
                   G_CALLBACK(webview_load_changed_cb), w);
  gtk_container_add(GTK_CONTAINER(w->priv.scroller), w->priv.webview);

  if (w->debug) {
    WebKitSettings *settings =
        webkit_web_view_get_settings(WEBKIT_WEB_VIEW(w->priv.webview));
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, true);
    webkit_settings_set_enable_developer_extras(settings, true);
  } else {
    g_signal_connect(G_OBJECT(w->priv.webview), "context-menu",
                     G_CALLBACK(webview_context_menu_cb), w);
  }

  if (!w->hidden) {
    gtk_widget_show_all(w->priv.window);
  }

  webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(w->priv.webview),
    REGISTER_EXTERNAL_INVOKE_JS, NULL, NULL, NULL);

  g_signal_connect(G_OBJECT(w->priv.window), "destroy",
                   G_CALLBACK(webview_destroy_cb), w);
  return 0;
}

WEBVIEW_API int webview_loop(struct webview *w, int blocking) {
  gtk_main_iteration_do(blocking);
  return w->priv.should_exit;
}

WEBVIEW_API void webview_set_title(struct webview *w, const char *title) {
  gtk_window_set_title(GTK_WINDOW(w->priv.window), title);
}

WEBVIEW_API void webview_set_fullscreen(struct webview *w, int fullscreen) {
  if (fullscreen) {
    gtk_window_fullscreen(GTK_WINDOW(w->priv.window));
  } else {
    gtk_window_unfullscreen(GTK_WINDOW(w->priv.window));
  }
}

WEBVIEW_API void webview_set_color(struct webview *w, uint8_t r, uint8_t g,
                                   uint8_t b, uint8_t a) {
  GdkRGBA color = {r / 255.0, g / 255.0, b / 255.0, a / 255.0};
  webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(w->priv.webview),
                                       &color);
}

WEBVIEW_API void webview_show_window(struct webview *w, int show)
{
  if (show)
    gtk_widget_show_all(GTK_WINDOW(w->priv.window));
  else
    gtk_widget_hide(GTK_WINDOW(w->priv.window));
}

WEBVIEW_API void webview_destroy_window(struct webview *w) {
  gtk_widget_destroy(GTK_WINDOW(w->priv.window));
}

WEBVIEW_API void webview_dialog(struct webview *w,
                                enum webview_dialog_type dlgtype, 
                                int flags,
                                const char *title, 
                                const char *arg,
                                char *result, 
                                size_t resultsz) {
  GtkWidget *dlg;
  if (result != NULL) {
    result[0] = '\0';
  }
  if (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN ||
      dlgtype == WEBVIEW_DIALOG_TYPE_SAVE) {
    dlg = gtk_file_chooser_dialog_new(
        title, GTK_WINDOW(w->priv.window),
        (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN
             ? (flags & WEBVIEW_DIALOG_FLAG_DIRECTORY
                    ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                    : GTK_FILE_CHOOSER_ACTION_OPEN)
             : GTK_FILE_CHOOSER_ACTION_SAVE),
        "_Cancel", GTK_RESPONSE_CANCEL,
        (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN ? "_Open" : "_Save"),
        GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dlg), FALSE);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dlg), FALSE);
    gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dlg), TRUE);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    gtk_file_chooser_set_create_folders(GTK_FILE_CHOOSER(dlg), TRUE);
    gint response = gtk_dialog_run(GTK_DIALOG(dlg));
    if (response == GTK_RESPONSE_ACCEPT) {
      gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
      g_strlcpy(result, filename, resultsz);
      g_free(filename);
    }
    gtk_widget_destroy(dlg);
  } else if (dlgtype == WEBVIEW_DIALOG_TYPE_ALERT) {
    GtkMessageType type = GTK_MESSAGE_OTHER;
    switch (flags & WEBVIEW_DIALOG_FLAG_ALERT_MASK) {
    case WEBVIEW_DIALOG_FLAG_INFO:
      type = GTK_MESSAGE_INFO;
      break;
    case WEBVIEW_DIALOG_FLAG_WARNING:
      type = GTK_MESSAGE_WARNING;
      break;
    case WEBVIEW_DIALOG_FLAG_ERROR:
      type = GTK_MESSAGE_ERROR;
      break;
    }
    dlg = gtk_message_dialog_new(GTK_WINDOW(w->priv.window), GTK_DIALOG_MODAL,
                                 type, GTK_BUTTONS_OK, "%s", title);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s",
                                             arg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
  }
}

static void webview_eval_finished(GObject *object, GAsyncResult *result,
                                  gpointer userdata) {
  (void)object;
  (void)result;
  struct webview *w = (struct webview *)userdata;
  w->priv.js_busy = 0;
}

WEBVIEW_API int webview_eval(struct webview *w, const char *js) {
  while (w->priv.ready == 0) {
    g_main_context_iteration(NULL, TRUE);
  }
  w->priv.js_busy = 1;
  webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(w->priv.webview), js, NULL,
                                 webview_eval_finished, w);
  while (w->priv.js_busy) {
    g_main_context_iteration(NULL, TRUE);
  }
  return 0;
}

static gboolean webview_dispatch_wrapper(gpointer userdata) {
  struct webview *w = (struct webview *)userdata;
  for (;;) {
    struct webview_dispatch_arg *arg =
        (struct webview_dispatch_arg *)g_async_queue_try_pop(w->priv.queue);
    if (arg == NULL) {
      break;
    }
    (arg->fn)(w, arg->arg);
    g_free(arg);
  }
  return FALSE;
}

WEBVIEW_API void webview_dispatch(struct webview *w, webview_dispatch_fn fn,
                                  void *arg) {
  struct webview_dispatch_arg *context =
      (struct webview_dispatch_arg *)g_new(struct webview_dispatch_arg, 1);
  context->w = w;
  context->arg = arg;
  context->fn = fn;
  g_async_queue_lock(w->priv.queue);
  g_async_queue_push_unlocked(w->priv.queue, context);
  if (g_async_queue_length_unlocked(w->priv.queue) == 1) {
    gdk_threads_add_idle(webview_dispatch_wrapper, w);
  }
  g_async_queue_unlock(w->priv.queue);
}

WEBVIEW_API void webview_terminate(struct webview *w) {
  w->priv.should_exit = 1;
}

WEBVIEW_API void webview_exit(struct webview *w) { (void)w; }
WEBVIEW_API void webview_print_log(const char *s) {
  fprintf(stderr, "%s\n", s);
}

WEBVIEW_API void webview_signal_connect(struct webview* w, const char *event_name, GCallback callback, void* arg) {
  g_signal_connect(G_OBJECT(w->priv.window),
      event_name, G_CALLBACK(callback), arg);
}
