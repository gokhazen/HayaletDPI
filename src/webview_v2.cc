/**
 * @file webview_v2.cc
 * @brief V2 WebView2 dashboard: API bridge, process enumeration, update checker.
 *
 * Implements the C++ side of the V2 UI:
 *  - JavaScript ↔ C engine callback bridge (v2_get_config, v2_save_config,
 *    v2_get_stats, v2_get_list, v2_save_list, v2_toggle_engine,
 *    v2_get_processes, v2_check_update, v2_open_url)
 *  - Asynchronous update check via WinHTTP → GitHub Releases API
 *  - Running process enumeration via CreateToolhelp32Snapshot
 *  - External URL launching via ShellExecuteA
 *  - LaunchV2Dashboard: creates the WebView2 window on a dedicated thread
 *
 * Uses webview_core.h (single-header WebView2 wrapper).
 * Microsoft Edge WebView2 SDK: https://aka.ms/webview2
 * SDK EULA: https://aka.ms/webview2eula
 *
 * @copyright Copyright (C) 2024-2026 Gokhan Ozen (gokhazen)
 * SPDX-License-Identifier: MIT
 */
#define WEBVIEW_IMPLEMENTATION
#include "webview_core.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <winhttp.h>

// Extern links to gui.c state and functions
extern "C" {
    extern uint64_t packets_processed;
    extern uint64_t packets_circumvented;
    extern int engine_running;
    extern char* ini_file;
    extern char* log_file;
    
    void ToggleEngine();
    void RestartEngine();
    BOOL GetAutoStart();
    void SetAutoStart(BOOL enable);
}

static void cb_get_stats(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    char res[256];
    int ratio = packets_processed > 0 ? (int)((packets_circumvented * 100) / packets_processed) : 0;
    snprintf(res, sizeof(res), "{\"processed\":%llu,\"bypassed\":%llu,\"ratio\":%d,\"running\":%s}", 
             packets_processed, packets_circumvented, ratio, engine_running ? "true" : "false");
    webview_return(w, seq, 0, res);
}

static void cb_get_config(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    char mode[16]={}, dns_ip[256]={}, dns_port[16]={}, doh_url[512]={}, filter[16]={};
    GetPrivateProfileString("Settings", "Mode", "-5", mode, sizeof(mode), ini_file);
    GetPrivateProfileString("Settings", "FilterMode", "0", filter, sizeof(filter), ini_file);
    GetPrivateProfileString("Settings", "DNSAddr", "1.1.1.1", dns_ip, sizeof(dns_ip), ini_file);
    GetPrivateProfileString("Settings", "DNSPort", "53", dns_port, sizeof(dns_port), ini_file);
    GetPrivateProfileString("Settings", "DOHUrl", "", doh_url, sizeof(doh_url), ini_file);
    
    char* json = (char*)malloc(1024);
    snprintf(json, 1024, "{\"mode\":\"%s\",\"filter_mode\":\"%s\",\"dns_ip\":\"%s\",\"dns_port\":\"%s\",\"doh_url\":\"%s\"}",
            mode, filter, dns_ip, dns_port, doh_url);
    webview_return(w, seq, 0, json);
    free(json);
}

static void cb_toggle(const char *seq, const char *req, void *arg) {
    ToggleEngine();
    webview_return((webview_t)arg, seq, 0, "{\"success\":true}");
}

static void cb_get_logs(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    FILE* f = fopen(log_file, "r");
    if (!f) {
        webview_return(w, seq, 0, "\"No logs available.\"");
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size > 16384) { fseek(f, size - 16384, SEEK_SET); size = 16384; }
    else fseek(f, 0, SEEK_SET);
    
    char* buf = (char*)malloc(size + 1);
    size_t r = fread(buf, 1, size, f);
    buf[r] = 0;
    fclose(f);
    
    char* esc = (char*)malloc(size * 4 + 10);
    int p = 0;
    esc[p++] = '"';
    for(int i=0; i<(int)r; i++){
        if(buf[i]=='\n') { esc[p++]='\\'; esc[p++]='n'; }
        else if(buf[i]=='\r') { /* skip */ }
        else if(buf[i]=='\\') { esc[p++]='\\'; esc[p++]='\\'; }
        else if(buf[i]=='"') { esc[p++]='\\'; esc[p++]='"'; }
        else { esc[p++]=buf[i]; }
    }
    esc[p++] = '"';
    esc[p] = 0;
    webview_return(w, seq, 0, esc);
    free(buf);
    free(esc);
}

static void cb_get_list(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    char path[256];
    if (strstr(req, "blacklist")) strcpy(path, "userfiles\\blacklist.txt");
    else if (strstr(req, "allowedapps")) strcpy(path, "userfiles\\allowedapps.txt");
    else strcpy(path, "userfiles\\allowlist.txt");
    
    FILE* f = fopen(path, "r");
    if (!f) { webview_return(w, seq, 0, "[]"); return; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    if(size <= 0) { fclose(f); webview_return(w, seq, 0, "[]"); return; }
    char* buf = (char*)malloc(size + 1);
    size_t r = fread(buf, 1, size, f); buf[r] = 0; fclose(f);
    
    char* json = (char*)malloc(size * 2 + 10);
    int p = 0;
    json[p++] = '[';
    char* line = strtok(buf, "\r\n");
    int first = 1;
    while(line) {
        if(strlen(line) == 0) { line = strtok(NULL, "\r\n"); continue; }
        if(!first) { json[p++] = ','; }
        json[p++] = '"';
        int len = strlen(line);
        for(int i=0; i<len; i++) { if(line[i]=='\\'||line[i]=='"') json[p++]='\\'; json[p++]=line[i]; }
        json[p++] = '"';
        first = 0;
        line = strtok(NULL, "\r\n");
    }
    json[p++] = ']'; json[p] = 0;
    webview_return(w, seq, 0, json);
    free(buf); free(json);
}

// Get list of running processes for App Selector
static void cb_get_processes(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    
    // Use a set-like structure to avoid duplicates - simple sorted unique list
    char** names = (char**)malloc(sizeof(char*) * 1024);
    int count = 0;
    
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                // Skip system processes
                if (strcmp(pe.szExeFile, "System") == 0 ||
                    strcmp(pe.szExeFile, "Idle") == 0 ||
                    strcmp(pe.szExeFile, "[System Process]") == 0 ||
                    strcmp(pe.szExeFile, "Registry") == 0) continue;
                
                // Check for duplicate
                int dup = 0;
                for (int i = 0; i < count; i++) {
                    if (_stricmp(names[i], pe.szExeFile) == 0) { dup = 1; break; }
                }
                if (!dup && count < 1023) {
                    names[count] = _strdup(pe.szExeFile);
                    count++;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }
    
    // Build JSON array
    size_t jsize = count * 64 + 10;
    char* json = (char*)malloc(jsize);
    int p = 0;
    json[p++] = '[';
    for (int i = 0; i < count; i++) {
        if (i > 0) json[p++] = ',';
        json[p++] = '"';
        int len = strlen(names[i]);
        for (int j = 0; j < len && p < (int)jsize - 5; j++) {
            if (names[i][j] == '"' || names[i][j] == '\\') json[p++] = '\\';
            json[p++] = names[i][j];
        }
        json[p++] = '"';
        free(names[i]);
    }
    json[p++] = ']';
    json[p] = 0;
    free(names);
    
    webview_return(w, seq, 0, json);
    free(json);
}

extern "C" void RestartEngine();

static void extract_json_string(const char* json, int index, char* out, int max_len) {
    out[0] = 0;
    const char* p = json;
    for (int i = 0; i <= index; i++) {
        p = strchr(p, '"');
        if (!p) return;
        p++;
        if (i == index) {
            int j = 0;
            while (*p && *p != '"' && j < max_len - 1) {
                if (*p == '\\' && *(p+1)) { p++; }
                out[j++] = *p++;
            }
            out[j] = 0;
            return;
        }
        p = strchr(p, '"');
        if (!p) return;
        p++;
    }
}

static void cb_save_config(const char *seq, const char *req, void *arg) {
    char m[32]={}, fm[32]={}, dip[256]={}, dp[32]={}, doh[512]={};
    extract_json_string(req, 0, m, sizeof(m));
    extract_json_string(req, 1, fm, sizeof(fm));
    extract_json_string(req, 2, dip, sizeof(dip));
    extract_json_string(req, 3, dp, sizeof(dp));
    extract_json_string(req, 4, doh, sizeof(doh));
    
    WritePrivateProfileString("Settings", "Mode", m, ini_file);
    WritePrivateProfileString("Settings", "FilterMode", fm, ini_file);
    WritePrivateProfileString("Settings", "DNSAddr", dip, ini_file);
    WritePrivateProfileString("Settings", "DNSPort", dp, ini_file);
    WritePrivateProfileString("Settings", "DOHUrl", doh, ini_file);
    
    RestartEngine();
    webview_return((webview_t)arg, seq, 0, "{\"success\":true}");
}

static void cb_save_list(const char *seq, const char *req, void *arg) {
    char type[32]={};
    extract_json_string(req, 0, type, sizeof(type));
    
    char path[256];
    if (strstr(type, "blacklist")) strcpy(path, "userfiles\\blacklist.txt");
    else if (strstr(type, "allowedapps")) strcpy(path, "userfiles\\allowedapps.txt");
    else strcpy(path, "userfiles\\allowlist.txt");
    
    const char *p = strchr(req, '"');
    if (p) {
        p = strchr(p+1, '"');
        if (p) {
            p = strchr(p+1, '"');
            if (p) {
                p++;
                FILE* f = fopen(path, "w");
                if (f) {
                    while (*p && *p != '"') {
                        if (*p == '\\' && *(p+1) == 'n') { fputc('\n', f); p += 2; }
                        else if (*p == '\\' && *(p+1) == 'r') { p += 2; }
                        else if (*p == '\\' && *(p+1) == '"') { fputc('"', f); p += 2; }
                        else if (*p == '\\' && *(p+1) == '\\') { fputc('\\', f); p += 2; }
                        else { fputc(*p, f); p++; }
                    }
                    fclose(f);
                }
            }
        }
    }
    
    RestartEngine();
    webview_return((webview_t)arg, seq, 0, "{\"success\":true}");
}

static void cb_open_url(const char *seq, const char *req, void *arg) {
    char url[1024]={};
    extract_json_string(req, 0, url, sizeof(url));
    if (strlen(url) > 0) {
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    }
    webview_return((webview_t)arg, seq, 0, "{\"success\":true}");
}

static void cb_get_autostart(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    char res[64];
    snprintf(res, sizeof(res), "{\"enabled\":%s}", GetAutoStart() ? "true" : "false");
    webview_return(w, seq, 0, res);
}

static void cb_set_autostart(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    int enable = strstr(req, "true") != NULL;
    SetAutoStart(enable);
    webview_return(w, seq, 0, "{\"success\":true}");
}

// Check for updates via GitHub releases API
static DWORD WINAPI UpdateCheckThread(LPVOID p) {
    HINTERNET hSession = WinHttpOpen(L"HayaletDPI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { free(p); return 0; }
    
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); free(p); return 0; }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/repos/gokhazen/HayaletDPI/releases/latest",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); free(p); return 0; }
    
    BOOL sent = WinHttpSendRequest(hRequest, L"User-Agent: HayaletDPI\r\n", -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (sent) WinHttpReceiveResponse(hRequest, NULL);
    
    char buf[8192] = {};
    DWORD read = 0;
    WinHttpReadData(hRequest, buf, sizeof(buf)-1, &read);
    buf[read] = 0;
    
    // Parse "tag_name":"v0.6.0" from response
    char ver[64] = "unknown";
    char *tag = strstr(buf, "\"tag_name\"");
    if (tag) {
        tag = strchr(tag, ':');
        if (tag) {
            tag = strchr(tag, '"'); 
            if (tag) {
                tag++;
                int i = 0;
                while (*tag && *tag != '"' && i < 63) ver[i++] = *tag++;
                ver[i] = 0;
            }
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    // Return result via JS dispatch
    webview_t w = (webview_t)p;
    char js[256];
    // Escape ver
    snprintf(js, sizeof(js), "window._updateResult && window._updateResult(\"%s\");", ver);
    webview_dispatch(w, [](webview_t wv, void* arg){ 
        // Executed on main thread - arg is prebuilt JS string
        webview_eval(wv, (const char*)arg);
    }, (void*)_strdup(js));
    
    return 0;
}

static void cb_check_update(const char *seq, const char *req, void *arg) {
    webview_t w = (webview_t)arg;
    // Acknowledge immediately, result will come async
    webview_return(w, seq, 0, "{\"checking\":true}");
    CreateThread(NULL, 0, UpdateCheckThread, (LPVOID)w, 0, NULL);
}

DWORD WINAPI V2Thread(LPVOID param) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    char *lastBs = strrchr(exePath, '\\');
    if (lastBs) *lastBs = '\0';
    
    char htmlPath[MAX_PATH];
    snprintf(htmlPath, MAX_PATH, "%s\\ui_v2\\index.html", exePath);
    if (GetFileAttributes(htmlPath) == INVALID_FILE_ATTRIBUTES) {
        snprintf(htmlPath, MAX_PATH, "%s\\..\\..\\ui_v2\\index.html", exePath);
    }
    for (int i = 0; htmlPath[i]; i++) {
        if (htmlPath[i] == '\\') htmlPath[i] = '/';
    }
    
    char url[MAX_PATH+10];
    snprintf(url, sizeof(url), "file:///%s", htmlPath);
    
    webview_t w = webview_create(1, NULL);
    if (!w) {
        MessageBox(NULL, "Failed to initialize WebView2.\nPlease install Microsoft Edge WebView2 Runtime.", "HayaletDPI Error", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }
    webview_set_title(w, "HayaletDPI V2 Pro");
    webview_set_size(w, 1050, 720, WEBVIEW_HINT_NONE);
    
    webview_bind(w, "v2_get_stats",     cb_get_stats,     w);
    webview_bind(w, "v2_get_config",    cb_get_config,    w);
    webview_bind(w, "v2_save_config",   cb_save_config,   w);
    webview_bind(w, "v2_get_logs",      cb_get_logs,      w);
    webview_bind(w, "v2_toggle_engine", cb_toggle,        w);
    webview_bind(w, "v2_get_list",      cb_get_list,      w);
    webview_bind(w, "v2_save_list",     cb_save_list,     w);
    webview_bind(w, "v2_open_url",      cb_open_url,      w);
    webview_bind(w, "v2_get_processes", cb_get_processes, w);
    webview_bind(w, "v2_check_update",  cb_check_update,  w);
    webview_bind(w, "v2_get_autostart", cb_get_autostart, w);
    webview_bind(w, "v2_set_autostart", cb_set_autostart, w);
    
    webview_navigate(w, url);
    webview_run(w);
    webview_destroy(w);
    
    CoUninitialize();
    return 0;
}

extern "C" void LaunchV2Dashboard(HWND parent) {
    CreateThread(NULL, 0, V2Thread, NULL, 0, NULL);
}
