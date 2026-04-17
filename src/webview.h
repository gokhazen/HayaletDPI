#ifndef WEBVIEW_H
#define WEBVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *webview_t;

enum webview_hint_t {
  WEBVIEW_HINT_NONE = 0,
  WEBVIEW_HINT_MIN = 1,
  WEBVIEW_HINT_MAX = 2,
  WEBVIEW_HINT_FIXED = 3
};

webview_t webview_create(int debug, void *window);
void webview_destroy(webview_t w);
void webview_run(webview_t w);
void webview_terminate(webview_t w);
void webview_set_title(webview_t w, const char *title);
void webview_set_size(webview_t w, int width, int height, int hint);
void webview_navigate(webview_t w, const char *url);
void webview_init(webview_t w, const char *js);
void webview_eval(webview_t w, const char *js);
void webview_dispatch(webview_t w, void (*fn)(webview_t, void *), void *arg);
void webview_bind(webview_t w, const char *name, void (*fn)(const char *, const char *, void *), void *arg);
void webview_unbind(webview_t w, const char *name);
void webview_return(webview_t w, const char *seq, int status, const char *result);

#ifdef __cplusplus
}
#endif

#endif /* WEBVIEW_H */
