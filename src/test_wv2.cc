#include <windows.h>
#define WEBVIEW_IMPLEMENTATION
#include "webview_core.h"
#include <iostream>

int main() {
    try {
        std::cout << "Starting WebView2 creation..." << std::endl;
        webview_t w = webview_create(1, NULL);
        if (!w) {
            std::cout << "webview_create returned NULL!" << std::endl;
            return 1;
        }
        std::cout << "webview created successfully. setting title..." << std::endl;
        webview_set_title(w, "Test");
        std::cout << "setting size..." << std::endl;
        webview_set_size(w, 800, 600, WEBVIEW_HINT_NONE);
        std::cout << "navigating..." << std::endl;
        webview_bind(w, "test_bind", [](const char* s, const char* r, void* a) {
            webview_return((webview_t)a, s, 0, "{\"hello\":\"world\"}");
        }, w);
        webview_navigate(w, "data:text/html,<html><body><script>async function f() { try { let r = await window.test_bind(); document.body.innerHTML += typeof r + ' ' + (typeof r === 'object' ? JSON.stringify(r) : r); } catch(e) { document.body.innerHTML += 'Error: ' + e; } } f();</script></body></html>");
        webview_run(w);
        std::cout << "Destroying webview." << std::endl;
        webview_destroy(w);
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unknown exception caught!" << std::endl;
    }
    return 1;
}
