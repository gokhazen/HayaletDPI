#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>
#include "hayalet.h"
#include "version.h"
#include "resource.h"
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1
#define IDM_EXIT 101
#define IDM_SETTINGS 102
#define IDM_RESTART 103
#define IDM_TOGGLE 104
#define IDM_VIEW_LOGS 105

HANDLE hThread = NULL;
DWORD threadId;
char* ini_file = USER_FILES_PATH "settings.ini";
char* log_file = USER_FILES_PATH "hayalet.log";
char* bl_path = USER_FILES_PATH "blacklist.txt";
int engine_running = 0;
HWND main_hwnd = NULL;
HINSTANCE hInst = NULL;
NOTIFYICONDATA nid = {0};
HBRUSH hbrBkgnd = NULL;
HBRUSH hbrEdit = NULL;

#define GRAPH_STEPS 60
static uint64_t traffic_history[GRAPH_STEPS] = {0};
static uint64_t bypass_history[GRAPH_STEPS] = {0};
static uint64_t last_processed_count = 0;
static uint64_t last_circumvent_count = 0;

void DrawTrafficGraph(HWND hwnd, HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    
    HBRUSH hbg = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(hdc, &rect, hbg);
    DeleteObject(hbg);

    HPEN pGrid = CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
    SelectObject(hdc, pGrid);
    for(int i=0; i<rect.bottom; i+=20) { MoveToEx(hdc, 0, i, NULL); LineTo(hdc, rect.right, i); }
    for(int i=0; i<rect.right; i+=20) { MoveToEx(hdc, i, 0, NULL); LineTo(hdc, i, rect.bottom); }
    DeleteObject(pGrid);

    uint64_t max_v = 10;
    for(int i=0; i<GRAPH_STEPS; i++) if(traffic_history[i] > max_v) max_v = traffic_history[i];

    HPEN pTraffic = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
    HPEN pBypass = CreatePen(PS_SOLID, 2, RGB(0, 200, 80));
    
    float dx = (float)rect.right / (GRAPH_STEPS - 1);
    float dy = (float)(rect.bottom - 10) / (float)max_v;

    SelectObject(hdc, pTraffic);
    for(int i=0; i<GRAPH_STEPS-1; i++) {
        MoveToEx(hdc, (int)(i*dx), rect.bottom - (int)(traffic_history[i]*dy) - 5, NULL);
        LineTo(hdc, (int)((i+1)*dx), rect.bottom - (int)(traffic_history[i+1]*dy) - 5);
    }

    SelectObject(hdc, pBypass);
    for(int i=0; i<GRAPH_STEPS-1; i++) {
        MoveToEx(hdc, (int)(i*dx), rect.bottom - (int)(bypass_history[i]*dy) - 5, NULL);
        LineTo(hdc, (int)((i+1)*dx), rect.bottom - (int)(bypass_history[i+1]*dy) - 5);
    }

    DeleteObject(pTraffic);
    DeleteObject(pBypass);
}

void SetImmersiveDarkMode(HWND hwnd) {
    BOOL value = FALSE; // Reverted to Light Mode
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}

void SetAutoStart(BOOL enable) {
    HKEY hKey;
    char path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) RegSetValueEx(hKey, "HayaletEngine", 0, REG_SZ, (BYTE*)path, (DWORD)strlen(path)+1);
        else RegDeleteValue(hKey, "HayaletEngine");
        RegCloseKey(hKey);
    }
}

BOOL GetAutoStart() {
    HKEY hKey;
    char val[MAX_PATH];
    DWORD size = sizeof(val);
    BOOL exists = FALSE;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, "HayaletEngine", NULL, NULL, (BYTE*)val, &size) == ERROR_SUCCESS) exists = TRUE;
        RegCloseKey(hKey);
    }
    return exists;
}

void UpdateStatusText(HWND hwndDlg) {
    char processed[32], circum[32], ratio[32];
    sprintf(processed, "%llu", packets_processed);
    sprintf(circum, "%llu", packets_circumvented);
    
    double r = 0;
    if (packets_processed > 0) r = ((double)packets_circumvented / (double)packets_processed) * 100.0;
    sprintf(ratio, "%.1f%%", r);

    SetDlgItemText(hwndDlg, IDC_TRAFFIC_PROCESSED, processed);
    SetDlgItemText(hwndDlg, IDC_TRAFFIC_CIRCUMVENTED, circum);
    SetDlgItemText(hwndDlg, IDC_TRAFFIC_RATIO, ratio);

    uint64_t diff_proc = (packets_processed > last_processed_count) ? (packets_processed - last_processed_count) : 0;
    uint64_t diff_circ = (packets_circumvented > last_circumvent_count) ? (packets_circumvented - last_circumvent_count) : 0;
    last_processed_count = packets_processed;
    last_circumvent_count = packets_circumvented;

    memmove(traffic_history, traffic_history + 1, sizeof(uint64_t) * (GRAPH_STEPS - 1));
    memmove(bypass_history, bypass_history + 1, sizeof(uint64_t) * (GRAPH_STEPS - 1));
    traffic_history[GRAPH_STEPS - 1] = diff_proc;
    bypass_history[GRAPH_STEPS - 1] = diff_circ;

    HWND hGraph = GetDlgItem(hwndDlg, IDC_TRAFFIC_GRAPH);
    if (hGraph) {
        InvalidateRect(hGraph, NULL, TRUE);
    }

    if (engine_running) {
        SetDlgItemText(hwndDlg, IDC_STATUS_TEXT, "ENGINE: ACTIVE (ONLINE)");
    } else {
        SetDlgItemText(hwndDlg, IDC_STATUS_TEXT, "ENGINE: STOPPED (IDLE)");
    }
}

void UpdateTrayIcon() {
    if (engine_running) {
        nid.hIcon = LoadIcon(hInst, "id");
        if (!nid.hIcon) nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        strcpy(nid.szTip, "Hayalet v0.4.0 - Running");
    } else {
        nid.hIcon = LoadIcon(NULL, IDI_WARNING);
        strcpy(nid.szTip, "Hayalet v0.4.0 - Stopped");
    }
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void WriteDefaultSettings() {
    WritePrivateProfileString("Settings", "Mode", "-5", ini_file);
    WritePrivateProfileString("Settings", "FilterMode", "0", ini_file);
    WritePrivateProfileString("Settings", "DNSAddr", "1.1.1.1", ini_file);
    WritePrivateProfileString("Settings", "DNSPort", "53", ini_file);
    WritePrivateProfileString("Settings", "DNSv6Addr", "2606:4700:4700::1111", ini_file);
    WritePrivateProfileString("Settings", "DNSv6Port", "53", ini_file);
    WritePrivateProfileString("Settings", "Blacklist", "blacklist.txt", ini_file);
    WritePrivateProfileString("Settings", "DoH", "0", ini_file);
    WritePrivateProfileString("Settings", "CustomArgs", "", ini_file);
}

DWORD WINAPI dpi_thread(LPVOID lpParam) {
    if (GetFileAttributes(ini_file) == INVALID_FILE_ATTRIBUTES) {
        WriteDefaultSettings();
    }

    char mode[16] = {0};
    char dns_addr[64] = {0};
    char dns_port[16] = {0};
    char dnsv6_addr[64] = {0};
    char dnsv6_port[16] = {0};
    char blacklist[256] = {0};
    char custom_args[512] = {0};
    char filter_mode[8] = "0";

    GetPrivateProfileString("Settings", "Mode", "-5", mode, sizeof(mode), ini_file);
    GetPrivateProfileString("Settings", "FilterMode", "0", filter_mode, sizeof(filter_mode), ini_file);
    GetPrivateProfileString("Settings", "DNSAddr", "", dns_addr, sizeof(dns_addr), ini_file);
    GetPrivateProfileString("Settings", "DNSPort", "", dns_port, sizeof(dns_port), ini_file);
    GetPrivateProfileString("Settings", "DNSv6Addr", "", dnsv6_addr, sizeof(dnsv6_addr), ini_file);
    GetPrivateProfileString("Settings", "DNSv6Port", "", dnsv6_port, sizeof(dnsv6_port), ini_file);
    GetPrivateProfileString("Settings", "Blacklist", "blacklist.txt", blacklist, sizeof(blacklist), ini_file);
    char doh[8] = "0"; GetPrivateProfileString("Settings", "DoH", "0", doh, sizeof(doh), ini_file);
    GetPrivateProfileString("Settings", "CustomArgs", "", custom_args, sizeof(custom_args), ini_file);
    char allowlist_en[8] = "0"; GetPrivateProfileString("Settings", "AllowlistEnabled", "0", allowlist_en, sizeof(allowlist_en), ini_file);

    int argc = 1;
    char *argv[100];
    argv[0] = "hayalet.exe";

    if (strlen(mode) > 0) argv[argc++] = strdup(mode);
    if (strlen(dns_addr) > 0) { argv[argc++] = strdup("--dns-addr"); argv[argc++] = strdup(dns_addr); }
    if (strlen(dns_port) > 0) { argv[argc++] = strdup("--dns-port"); argv[argc++] = strdup(dns_port); }
    if (strlen(dnsv6_addr) > 0) { argv[argc++] = strdup("--dnsv6-addr"); argv[argc++] = strdup(dnsv6_addr); }
    if (strlen(dnsv6_port) > 0) { argv[argc++] = strdup("--dnsv6-port"); argv[argc++] = strdup(dnsv6_port); }
    if (filter_mode[0] == '1' && strlen(blacklist) > 0) { argv[argc++] = strdup("--blacklist"); argv[argc++] = strdup(blacklist); }
    argv[argc++] = strdup("--filter-mode"); argv[argc++] = strdup(filter_mode);
    if (allowlist_en[0] == '1') { argv[argc++] = strdup("-A"); argv[argc++] = strdup("1"); }

    if (strlen(custom_args) > 0) {
        char* token = strtok(custom_args, " ");
        while (token && argc < 98) {
            argv[argc++] = strdup(token);
            token = strtok(NULL, " ");
        }
    }
    argv[argc] = NULL;
    
    reset_hayalet_state();
    engine_running = 1;
    UpdateTrayIcon();
    run_hayalet(argc, argv);
    engine_running = 0;
    UpdateTrayIcon();
    for (int i = 1; i < argc; i++) free(argv[i]);
    return 0;
}

void StopEngine() {
    if (engine_running && hThread) {
        stop_hayalet();
        shutdown_hayalet();
        WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);
        hThread = NULL;
        deinit_all();
        engine_running = 0;
        UpdateTrayIcon();
    }
}

void StartEngine() {
    if (!engine_running) hThread = CreateThread(NULL, 0, dpi_thread, NULL, 0, &threadId);
}

void RestartEngine() { StopEngine(); Sleep(500); StartEngine(); }
void ToggleEngine() { if (engine_running) StopEngine(); else StartEngine(); }

INT_PTR CALLBACK LogViewerDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HFONT hFontMono = NULL;
    switch (uMsg) {
        case WM_INITDIALOG: {
            SetImmersiveDarkMode(hwndDlg);
            hFontMono = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
            if (hFontMono) SendDlgItemMessage(hwndDlg, IDC_EDIT_LOG_LIVE, WM_SETFONT, (WPARAM)hFontMono, TRUE);
            SetTimer(hwndDlg, 2, 500, NULL);
            return TRUE;
        }
        case WM_CTLCOLORDLG: return (INT_PTR)hbrBkgnd;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: { SetTextColor((HDC)wParam, RGB(0,0,0)); SetBkColor((HDC)wParam, RGB(255,255,255)); return (INT_PTR)hbrBkgnd; }
        case WM_TIMER:
            if (wParam == 2) {
                static long last_pos = 0;
                FILE* f = fopen(log_file, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long current_size = ftell(f);
                    if (current_size < last_pos) {
                        last_pos = 0;
                        SetDlgItemText(hwndDlg, IDC_EDIT_LOG_LIVE, "");
                    }
                    if (current_size > last_pos) {
                        fseek(f, last_pos, SEEK_SET);
                        long to_read = current_size - last_pos;
                        if (to_read > 8192) to_read = 8192;
                        char* buf = malloc(to_read + 1);
                        size_t read = fread(buf, 1, to_read, f);
                        buf[read] = 0;
                        last_pos += (long)read;
                        
                        HWND hEdit = GetDlgItem(hwndDlg, IDC_EDIT_LOG_LIVE);
                        int len = GetWindowTextLength(hEdit);
                        
                        // Check if we should autoscroll: only if we're near the end
                        SCROLLINFO si = {sizeof(si), SIF_ALL};
                        GetScrollInfo(hEdit, SB_VERT, &si);
                        BOOL atBottom = (si.nPos + (int)si.nPage >= si.nMax - 1);
                        
                        SendMessage(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                        SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)buf);
                        
                        if (atBottom) SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
                        free(buf);
                    }
                    fclose(f);
                }
            }
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BTN_CLEAR_LOG) { FILE* f = fopen(log_file, "wb"); if(f) fclose(f); return TRUE; }
            if (LOWORD(wParam) == IDCANCEL) { KillTimer(hwndDlg, 2); if (hFontMono) DeleteObject(hFontMono); hFontMono = NULL; EndDialog(hwndDlg, IDCANCEL); return TRUE; }
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK AppSelectorDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static char saved_apps[1024][MAX_PATH];
    static int saved_apps_count = 0;
    switch (uMsg) {
        case WM_INITDIALOG:
            SetImmersiveDarkMode(hwndDlg);
            saved_apps_count = 0;
            FILE* f = fopen(USER_FILES_PATH "allowedapps.txt", "r");
            if (f) {
                char line[MAX_PATH];
                while (fgets(line, MAX_PATH, f) && saved_apps_count < 1024) {
                    size_t len = strlen(line); while(len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) { line[len-1] = '\0'; len--; }
                    if (len > 0) strcpy(saved_apps[saved_apps_count++], line);
                }
                fclose(f);
            }
            SendMessage(hwndDlg, WM_COMMAND, IDC_BTN_REFRESH_APPS, 0);
            return TRUE;
        case WM_CTLCOLORDLG: return (INT_PTR)hbrBkgnd;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: { SetTextColor((HDC)wParam, RGB(0,0,0)); SetBkColor((HDC)wParam, RGB(255,255,255)); return (INT_PTR)hbrBkgnd; }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BTN_REFRESH_APPS) {
                HWND hList = GetDlgItem(hwndDlg, IDC_LIST_APPS); SendMessage(hList, LB_RESETCONTENT, 0, 0);
                DWORD windowPIDs[1024]; int windowPIDCount = 0; HWND currentWnd = GetTopWindow(NULL);
                while (currentWnd) { if (IsWindowVisible(currentWnd)) { DWORD pid; GetWindowThreadProcessId(currentWnd, &pid); BOOL fnd = FALSE; for(int j=0; j<windowPIDCount; j++) if(windowPIDs[j] == pid) fnd = TRUE; if(!fnd && windowPIDCount < 1024) windowPIDs[windowPIDCount++] = pid; } currentWnd = GetNextWindow(currentWnd, GW_HWNDNEXT); }
                HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if (hSnap != INVALID_HANDLE_VALUE) {
                    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
                    if (Process32First(hSnap, &pe)) {
                        do {
                            BOOL isApp = FALSE; for(int j=0; j<windowPIDCount; j++) if(windowPIDs[j] == pe.th32ProcessID) isApp = TRUE;
                            if (isApp) { int idx = (int)SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)pe.szExeFile); if (idx != LB_ERR) { for(int k=0; k<saved_apps_count; k++) if(_stricmp(pe.szExeFile, saved_apps[k]) == 0) { SendMessage(hList, LB_SETSEL, TRUE, (LPARAM)idx); break; } } }
                        } while (Process32Next(hSnap, &pe));
                    }
                    CloseHandle(hSnap);
                }
                return TRUE;
            }
            if (LOWORD(wParam) == IDOK) {
                HWND hList = GetDlgItem(hwndDlg, IDC_LIST_APPS); int cnt = (int)SendMessage(hList, LB_GETSELCOUNT, 0, 0);
                if (cnt > 0) {
                    int *idx = malloc(sizeof(int) * (size_t)cnt); SendMessage(hList, LB_GETSELITEMS, (WPARAM)cnt, (LPARAM)idx);
                    FILE *fp = fopen(USER_FILES_PATH "allowedapps.txt", "w"); if (fp) { for (int i=0; i<cnt; i++) { char name[256]; SendMessage(hList, LB_GETTEXT, idx[i], (LPARAM)name); if (name[0] != '-') fprintf(fp, "%s\n", name); } fclose(fp); }
                    if(idx) free(idx);
                } else { FILE *fp = fopen(USER_FILES_PATH "allowedapps.txt", "w"); if (fp) fclose(fp); }
                EndDialog(hwndDlg, IDOK); return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) { EndDialog(hwndDlg, IDCANCEL); return TRUE; }
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK TesterDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int best_row   = -1;
    static int best_score = -1;
    switch (uMsg) {
        case WM_INITDIALOG: {
            best_row = -1; best_score = -1;
            SetImmersiveDarkMode(hwndDlg);
            SetDlgItemText(hwndDlg, IDC_EDIT_TEST_URL, "discord.com");
            HWND hList = GetDlgItem(hwndDlg, IDC_LIST_TEST);
            SendMessage(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            LVCOLUMN lvc; lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; lvc.fmt = LVCFMT_LEFT;
            lvc.pszText = "Engine Mode";    lvc.cx = 88;  ListView_InsertColumn(hList, 0, &lvc);
            lvc.pszText = "TCP/HTTP";        lvc.cx = 62;  ListView_InsertColumn(hList, 1, &lvc);
            lvc.pszText = "TCP/HTTPS";       lvc.cx = 62;  ListView_InsertColumn(hList, 2, &lvc);
            lvc.pszText = "UDP/DNS";         lvc.cx = 62;  ListView_InsertColumn(hList, 3, &lvc);
            lvc.pszText = "QUIC";            lvc.cx = 55;  ListView_InsertColumn(hList, 4, &lvc);
            lvc.pszText = "Best ms";         lvc.cx = 61;  ListView_InsertColumn(hList, 5, &lvc);
            lvc.pszText = "Score";           lvc.cx = 50;  ListView_InsertColumn(hList, 6, &lvc);
            return TRUE;
        }
        case WM_CTLCOLORDLG: return (INT_PTR)hbrBkgnd;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: { SetTextColor((HDC)wParam, RGB(0,0,0)); SetBkColor((HDC)wParam, RGB(255,255,255)); return (INT_PTR)hbrBkgnd; }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BTN_START_TEST) {
                best_row = -1; best_score = -1;
                HWND hList = GetDlgItem(hwndDlg, IDC_LIST_TEST); ListView_DeleteAllItems(hList);
                char url[256]; GetDlgItemText(hwndDlg, IDC_EDIT_TEST_URL, url, sizeof(url));
                if (strlen(url) == 0) { MessageBox(hwndDlg, "Please enter a target host.", "Error", MB_ICONERROR); return TRUE; }

                const char* mode_names[] = { "Phantom (-1)", "Specter (-2)", "Wraith (-3)", "Shadow (-4)", "Silverhand (-5)", "Ghost (-6)", "Echo (-7)", "Mirror (-8)", "Void (-9)" };

                // Resolve host
                struct hostent *he = gethostbyname(url);
                if (!he) { MessageBox(hwndDlg, "Failed to resolve host. Check connectivity.", "Error", MB_ICONERROR); return TRUE; }

                struct sockaddr_in srv_http, srv_https, srv_dns;
                memset(&srv_http, 0, sizeof(srv_http));
                srv_http.sin_family = AF_INET;
                srv_http.sin_addr   = *((struct in_addr *)he->h_addr);
                srv_https = srv_http;
                srv_dns   = srv_http;
                srv_http.sin_port  = htons(80);
                srv_https.sin_port = htons(443);
                srv_dns.sin_port   = htons(53);

                DWORD timeout_ms = 3000;
                SetCursor(LoadCursor(NULL, IDC_WAIT));

                for (int i = 0; i < 9; i++) {
                    LVITEM lvi; lvi.mask = LVIF_TEXT; lvi.iItem = i; lvi.iSubItem = 0;
                    lvi.pszText = (char*)mode_names[i];
                    ListView_InsertItem(hList, &lvi);

                    // --- TCP/HTTP test (port 80) ---
                    DWORD t0 = GetTickCount();
                    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    int r_http = connect(s, (struct sockaddr*)&srv_http, sizeof(srv_http));
                    DWORD lat_http = GetTickCount() - t0;
                    closesocket(s);
                    char http_buf[24];
                    if (r_http == 0) sprintf(http_buf, "OK %lums", lat_http);
                    else strcpy(http_buf, "TIMEOUT");
                    lvi.iSubItem = 1; lvi.pszText = http_buf;
                    ListView_SetItem(hList, &lvi);

                    // --- TCP/HTTPS test (port 443) ---
                    t0 = GetTickCount();
                    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    int r_https = connect(s, (struct sockaddr*)&srv_https, sizeof(srv_https));
                    DWORD lat_https = GetTickCount() - t0;
                    closesocket(s);
                    char https_buf[24];
                    if (r_https == 0) sprintf(https_buf, "OK %lums", lat_https);
                    else strcpy(https_buf, "TIMEOUT");
                    lvi.iSubItem = 2; lvi.pszText = https_buf;
                    ListView_SetItem(hList, &lvi);

                    // --- UDP/DNS ping test (port 53) ---
                    // Send a minimal DNS query for the target hostname
                    SOCKET us = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    setsockopt(us, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    setsockopt(us, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    // Minimal DNS query packet for "discord.com"
                    unsigned char dns_q[] = {
                        0x00, 0x01, 0x01, 0x00, // Transaction ID, flags
                        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 1 question
                        0x07, 'd','i','s','c','o','r','d', 0x03, 'c','o','m', 0x00,
                        0x00, 0x01, 0x00, 0x01  // type A, class IN
                    };
                    // Patch query with actual host label if short enough
                    unsigned char dns_reply[512];
                    t0 = GetTickCount();
                    int udp_ok = 0;
                    DWORD lat_dns = 9999;
                    if (sendto(us, (char*)dns_q, sizeof(dns_q), 0, (struct sockaddr*)&srv_dns, sizeof(srv_dns)) != SOCKET_ERROR) {
                        int rlen = recvfrom(us, (char*)dns_reply, sizeof(dns_reply), 0, NULL, NULL);
                        lat_dns = GetTickCount() - t0;
                        if (rlen > 0) udp_ok = 1;
                    } else {
                        lat_dns = timeout_ms;
                    }
                    closesocket(us);
                    char dns_buf[24];
                    if (udp_ok) sprintf(dns_buf, "OK %lums", lat_dns);
                    else strcpy(dns_buf, "NO RESP");
                    lvi.iSubItem = 3; lvi.pszText = dns_buf;
                    ListView_SetItem(hList, &lvi);

                    // --- QUIC (UDP 443) block check ---
                    SOCKET qs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    setsockopt(qs, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    setsockopt(qs, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
                    struct sockaddr_in srv_quic = srv_https;
                    srv_quic.sin_port = htons(443);
                    // Send a minimal QUIC Initial-like payload (gets dropped by engine in mode -9)
                    unsigned char quic_probe[] = { 0xC0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
                    DWORD tq0 = GetTickCount();
                    int quic_sent = (sendto(qs, (char*)quic_probe, sizeof(quic_probe), 0, (struct sockaddr*)&srv_quic, sizeof(srv_quic)) != SOCKET_ERROR);
                    DWORD lat_quic = GetTickCount() - tq0;
                    closesocket(qs);
                    char quic_buf[24];
                    sprintf(quic_buf, quic_sent ? "SENT %lums" : "BLOCKED", lat_quic);
                    lvi.iSubItem = 4; lvi.pszText = quic_buf;
                    ListView_SetItem(hList, &lvi);

                    // --- Best latency & Score ---
                    DWORD best_lat = 9999;
                    if (r_http == 0 && lat_http < best_lat) best_lat = lat_http;
                    if (r_https == 0 && lat_https < best_lat) best_lat = lat_https;
                    if (udp_ok && lat_dns < best_lat) best_lat = lat_dns;
                    char best_buf[24];
                    if (best_lat < 9999) sprintf(best_buf, "%lums", best_lat);
                    else strcpy(best_buf, "N/A");
                    lvi.iSubItem = 5; lvi.pszText = best_buf;
                    ListView_SetItem(hList, &lvi);

                    // Score: success bonus + speed bonus
                    int score = 0;
                    if (r_http == 0)  score += 25;
                    if (r_https == 0) score += 35;
                    if (udp_ok)       score += 20;
                    if (r_https == 0) {
                        if      (lat_https < 50)  score += 20;
                        else if (lat_https < 100) score += 15;
                        else if (lat_https < 200) score += 10;
                        else if (lat_https < 500) score += 5;
                    }
                    // Track best row
                    if (score > best_score) { best_score = score; best_row = i; }

                    char score_buf[16]; sprintf(score_buf, "%d/100", score > 100 ? 100 : score);
                    lvi.iSubItem = 6; lvi.pszText = score_buf;
                    ListView_SetItem(hList, &lvi);

                    UpdateWindow(hList);
                    MSG peek; while (PeekMessage(&peek, hwndDlg, 0, 0, PM_REMOVE)) { TranslateMessage(&peek); DispatchMessage(&peek); }
                }
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                // Force repaint so CustomDraw picks up best_row
                InvalidateRect(hList, NULL, FALSE);
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) { EndDialog(hwndDlg, IDCANCEL); return TRUE; }
            break;
        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;
            if (pnm->idFrom == IDC_LIST_TEST && pnm->code == NM_CUSTOMDRAW) {
                LPNMLVCUSTOMDRAW pCD = (LPNMLVCUSTOMDRAW)lParam;
                if (pCD->nmcd.dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (pCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    if (best_row >= 0 && (int)pCD->nmcd.dwItemSpec == best_row) {
                        pCD->clrTextBk = RGB(180, 240, 180);  // light green
                        pCD->clrText   = RGB(0, 80, 0);
                        return CDRF_NEWFONT;
                    }
                }
            }
            break;
        }
    }
    return FALSE;
}

INT_PTR CALLBACK BlacklistEditorDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // lParam = path to blacklist file (passed via DialogBoxParam)
    static char bl_path[MAX_PATH] = {0};
    switch (uMsg) {
        case WM_INITDIALOG: {
            SetImmersiveDarkMode(hwndDlg);
            if (lParam) strncpy(bl_path, (char*)lParam, MAX_PATH - 1);
            else strncpy(bl_path, "blacklist.txt", MAX_PATH - 1);
            HWND hList = GetDlgItem(hwndDlg, IDC_LIST_BL_EDITOR);
            FILE *f = fopen(bl_path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) { line[len-1] = '\0'; len--; }
                    if (len > 0) SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)line);
                }
                fclose(f);
            }
            return TRUE;
        }
        case WM_CTLCOLORDLG: return (INT_PTR)hbrBkgnd;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: { SetTextColor((HDC)wParam, RGB(0,0,0)); SetBkColor((HDC)wParam, RGB(255,255,255)); return (INT_PTR)hbrBkgnd; }
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDC_BTN_BL_ADD) {
                char item[256]; GetDlgItemText(hwndDlg, IDC_EDIT_BL_ITEM, item, sizeof(item));
                if (strlen(item) > 0) {
                    SendMessage(GetDlgItem(hwndDlg, IDC_LIST_BL_EDITOR), LB_ADDSTRING, 0, (LPARAM)item);
                    SetDlgItemText(hwndDlg, IDC_EDIT_BL_ITEM, "");
                }
                return TRUE;
            }
            if (id == IDC_BTN_BL_DEL) {
                HWND hList = GetDlgItem(hwndDlg, IDC_LIST_BL_EDITOR);
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) SendMessage(hList, LB_DELETESTRING, (WPARAM)sel, 0);
                return TRUE;
            }
            if (id == IDC_BTN_BL_SAVE) {
                HWND hList = GetDlgItem(hwndDlg, IDC_LIST_BL_EDITOR);
                int cnt = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
                FILE *f = fopen(bl_path, "w");
                if (f) {
                    for (int i = 0; i < cnt; i++) {
                        char line[256]; SendMessage(hList, LB_GETTEXT, (WPARAM)i, (LPARAM)line);
                        fprintf(f, "%s\n", line);
                    }
                    fclose(f);
                    MessageBox(hwndDlg, "Blacklist saved. Restart engine to apply.", "Saved", MB_ICONINFORMATION);
                }
                EndDialog(hwndDlg, IDOK); return TRUE;
            }
            if (id == IDCANCEL) { EndDialog(hwndDlg, IDCANCEL); return TRUE; }
            break;
        }
    }
    return FALSE;
}

INT_PTR CALLBACK AllowlistDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            SetImmersiveDarkMode(hwndDlg);
            HWND hList = GetDlgItem(hwndDlg, IDC_LIST_ALLOWLIST);
            FILE *f = fopen(USER_FILES_PATH "allowlist.txt", "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) { line[len-1] = '\0'; len--; }
                    if (len > 0) SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)line);
                }
                fclose(f);
            }
            return TRUE;
        }
        case WM_CTLCOLORDLG: return (INT_PTR)hbrBkgnd;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: { SetTextColor((HDC)wParam, RGB(0,0,0)); SetBkColor((HDC)wParam, RGB(255,255,255)); return (INT_PTR)hbrBkgnd; }
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDC_BTN_ALLOWLIST_ADD) {
                char item[256]; GetDlgItemText(hwndDlg, IDC_EDIT_ALLOWLIST_ITEM, item, sizeof(item));
                if (strlen(item) > 0) {
                    SendMessage(GetDlgItem(hwndDlg, IDC_LIST_ALLOWLIST), LB_ADDSTRING, 0, (LPARAM)item);
                    SetDlgItemText(hwndDlg, IDC_EDIT_ALLOWLIST_ITEM, "");
                }
                return TRUE;
            }
            if (id == IDC_BTN_ALLOWLIST_DEL) {
                HWND hList = GetDlgItem(hwndDlg, IDC_LIST_ALLOWLIST);
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) SendMessage(hList, LB_DELETESTRING, (WPARAM)sel, 0);
                return TRUE;
            }
            if (id == IDC_BTN_ALLOWLIST_SAVE) {
                HWND hList = GetDlgItem(hwndDlg, IDC_LIST_ALLOWLIST);
                int cnt = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
                FILE *f = fopen("allowlist.txt", "w");
                if (f) {
                    for (int i = 0; i < cnt; i++) {
                        char line[256]; SendMessage(hList, LB_GETTEXT, (WPARAM)i, (LPARAM)line);
                        fprintf(f, "%s\n", line);
                    }
                    fclose(f);
                }
                EndDialog(hwndDlg, IDOK); return TRUE;
            }
            if (id == IDCANCEL) { EndDialog(hwndDlg, IDCANCEL); return TRUE; }
            break;
        }
    }
    return FALSE;
}

void ShowTab(HWND hwndDlg, int index) {
    // Tab 1 - Dashboard
    int t1[] = {
        IDC_GRP_DASHBOARD,
        IDC_LBL_PROCESSED, IDC_TRAFFIC_PROCESSED,
        IDC_LBL_BYPASS,    IDC_TRAFFIC_CIRCUMVENTED,
        IDC_LBL_RATIO,     IDC_TRAFFIC_RATIO,
        IDC_LBL_TRAFFIC_FLOW, IDC_TRAFFIC_GRAPH
    };
    // Tab 2 - Engine
    int t2[] = {
        IDC_GRP_ENGINE,
        IDC_LBL_STEALTH,     IDC_COMBO_MODE,
        IDC_LBL_DNS_HEADER,
        IDC_LBL_SELECT_PROV, IDC_LIST_DNS_PROVIDERS,
        IDC_LBL_PROTO_TYPE,  IDC_RAD_DNS_STANDARD, IDC_RAD_DNS_SECURE,
        IDC_LBL_PROV_IP,     IDC_EDIT_DNS_IP,
        IDC_LBL_DNS_PORT,    IDC_EDIT_DNS_PORT,
        IDC_LBL_DOH_URL,     IDC_EDIT_CUSTOM_DOH,
        IDC_LBL_DNS_STATUS,
        1100,
        IDC_BTN_TEST,
        IDC_CHK_AUTOSTART
    };
    // Tab 3 - Filter (inline editors, named separator)
    int t3[] = {
        IDC_GRP_DNS,
        IDC_RAD_ALL,  IDC_RAD_BKL,  IDC_RAD_APPS, IDC_RAD_GAMING,
        IDC_BTN_APP_SELECTOR,
        IDC_LBL_GAMETIP,
        IDC_SEP_FILTER,
        IDC_LBL_ALLOWLIST_INFO, IDC_LIST_INLINE_AL, IDC_EDIT_INLINE_AL, IDC_BTN_INLINE_AL_ADD, IDC_BTN_INLINE_AL_DEL,
        IDC_LBL_INLINE_BL,      IDC_LIST_INLINE_BL, IDC_EDIT_INLINE_BL, IDC_BTN_INLINE_BL_ADD, IDC_BTN_INLINE_BL_DEL
    };
    // Tab 4 - About (all named IDs, no IDC_STATIC in body)
    int t4[] = {
        IDC_GRP_ADVANCED,
        IDC_LBL_CURBUILD, IDC_LBL_ABOUT_DEV,
        IDC_BTN_ABOUT_DEV, IDC_BTN_ABOUT_PROJ,
        IDC_SEP_ABOUT,
        IDC_LBL_ABOUT_FEATURES, IDC_LIST_FEATURES
    };

    int* g[] = { t1, t2, t3, t4 };
    int  s[] = {  9,  19,  19,  8 };

    for (int t = 0; t < 4; t++)
        for (int i = 0; i < s[t]; i++)
            ShowWindow(GetDlgItem(hwndDlg, g[t][i]), SW_HIDE);

    for (int i = 0; i < s[index]; i++)
        ShowWindow(GetDlgItem(hwndDlg, g[index][i]), SW_SHOW);
}

typedef struct {
    const char* name;
    const char* ipv4;
    const char* port;
    const char* doh_url;
} DNSPreset;

DNSPreset dns_table[] = {
    {"Cloudflare (Global)", "1.1.1.1", "53", "https://cloudflare-dns.com/dns-query"},
    {"Google (Public)", "8.8.8.8", "53", "https://dns.google/dns-query"},
    {"Quad9 (Secure)", "9.9.9.9", "994", "https://dns.quad9.net/dns-query"},
    {"AdGuard (AdBlock)", "94.140.14.14", "5353", "https://dns.adguard.com/dns-query"},
    {"OpenDNS (Cisco)", "208.67.222.222", "53", "https://doh.opendns.com/dns-query"},
    {"Yandex (Anti-Cens)", "77.88.8.8", "1253", "https://common.dot.yandex.net/dns-query"},
    {"CleanBrowsing", "185.228.168.168", "5353", "https://doh.cleanbrowsing.org/doh/family-filter/"},
    {"Control D", "76.76.2.0", "5353", "https://dns.controld.com/freedns"},
    {"Custom", "", "53", ""}
};

INT_PTR CALLBACK SettingsDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            SetImmersiveDarkMode(hwndDlg); SetTimer(hwndDlg, 1, 1000, NULL);
            if (!hbrBkgnd) hbrBkgnd = CreateSolidBrush(RGB(255,255,255));
            if (!hbrEdit) hbrEdit = CreateSolidBrush(RGB(255,255,255));
            char val[256]; HWND hTab = GetDlgItem(hwndDlg, IDC_TAB_MAIN);
            TCITEM tie; tie.mask = TCIF_TEXT;
            tie.pszText = "Dashboard"; TabCtrl_InsertItem(hTab, 0, &tie);
            tie.pszText = "Engine"; TabCtrl_InsertItem(hTab, 1, &tie);
            tie.pszText = "Filter"; TabCtrl_InsertItem(hTab, 2, &tie);
            tie.pszText = "About"; TabCtrl_InsertItem(hTab, 3, &tie);
            ShowTab(hwndDlg, 0);

            const char* modes[] = { "Phantom (-1)", "Specter (-2)", "Wraith (-3)", "Shadow (-4)", "Silverhand (-5)", "Ghost (-6)", "Echo (-7)", "Mirror (-8)", "Void (-9)", "Custom" };
            const char* mode_vals[] = { "-1", "-2", "-3", "-4", "-5", "-6", "-7", "-8", "-9", "0" };
            for (int i=0; i<10; i++) SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_MODE), CB_ADDSTRING, 0, (LPARAM)modes[i]);
            GetPrivateProfileString("Settings", "Mode", "-5", val, sizeof(val), ini_file);
            int m_idx = 4; // Default to Silverhand
            for(int i=0; i<10; i++) if(strcmp(val, mode_vals[i]) == 0) m_idx = i;
            SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_MODE), CB_SETCURSEL, m_idx, 0);

            GetPrivateProfileString("Settings", "FilterMode", "0", val, sizeof(val), ini_file);
            int f_mode = atoi(val);
            CheckDlgButton(hwndDlg, IDC_RAD_ALL, (f_mode == 0) ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwndDlg, IDC_RAD_BKL, (f_mode == 1) ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwndDlg, IDC_RAD_APPS, (f_mode == 2) ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwndDlg, IDC_RAD_GAMING, (f_mode == 3) ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwndDlg, IDC_CHK_AUTOSTART, GetAutoStart() ? BST_CHECKED : BST_UNCHECKED);

            for (int i=0; i<9; i++) SendMessage(GetDlgItem(hwndDlg, IDC_LIST_DNS_PROVIDERS), LB_ADDSTRING, 0, (LPARAM)dns_table[i].name);
            GetPrivateProfileString("Settings", "DNSPreset", "0", val, sizeof(val), ini_file);
            SendMessage(GetDlgItem(hwndDlg, IDC_LIST_DNS_PROVIDERS), LB_SETCURSEL, atoi(val), 0);

            GetPrivateProfileString("Settings", "DoH", "0", val, sizeof(val), ini_file);
            int is_doh = atoi(val);
            CheckDlgButton(hwndDlg, (is_doh) ? IDC_RAD_DNS_SECURE : IDC_RAD_DNS_STANDARD, BST_CHECKED);

            GetPrivateProfileString("Settings", "DNSAddr", "1.1.1.1", val, sizeof(val), ini_file); SetDlgItemText(hwndDlg, IDC_EDIT_DNS_IP, val);
            GetPrivateProfileString("Settings", "DNSPort", "53", val, sizeof(val), ini_file); SetDlgItemText(hwndDlg, IDC_EDIT_DNS_PORT, val);
            GetPrivateProfileString("Settings", "DOHUrl", dns_table[0].doh_url, val, sizeof(val), ini_file); SetDlgItemText(hwndDlg, IDC_EDIT_CUSTOM_DOH, val);
            
            // --- Load inline Filter lists ---
            {
                HWND hAL = GetDlgItem(hwndDlg, IDC_LIST_INLINE_AL);
                HWND hBL = GetDlgItem(hwndDlg, IDC_LIST_INLINE_BL);
                FILE *f;
                char line[256];

                f = fopen(USER_FILES_PATH "allowlist.txt", "r");
                if (f) {
                    while (fgets(line, sizeof(line), f)) {
                        size_t len = strlen(line);
                        while (len > 0 && (line[len-1]=='\r'||line[len-1]=='\n')) line[--len]='\0';
                        if (len > 0) SendMessage(hAL, LB_ADDSTRING, 0, (LPARAM)line);
                    }
                    fclose(f);
                }

                f = fopen(USER_FILES_PATH "blacklist.txt", "r");
                if (f) {
                    while (fgets(line, sizeof(line), f)) {
                        size_t len = strlen(line);
                        while (len > 0 && (line[len-1]=='\r'||line[len-1]=='\n')) line[--len]='\0';
                        if (len > 0) SendMessage(hBL, LB_ADDSTRING, 0, (LPARAM)line);
                    }
                    fclose(f);
                }

                // Allowlist disabled when Gaming or Blacklist mode
                BOOL al_enabled = (f_mode == 0 || f_mode == 2);
                EnableWindow(hAL,                                 al_enabled);
                EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_INLINE_AL),     al_enabled);
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_INLINE_AL_ADD),  al_enabled);
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_INLINE_AL_DEL),  al_enabled);
            }

            // --- Populate About feature list ---
            {
                HWND hFeat = GetDlgItem(hwndDlg, IDC_LIST_FEATURES);
                const char* features[] = {
                    "Multi-protocol DPI bypass (9 stealth engine modes)",
                    "Traffic graph: real-time 60-second throughput view",
                    "DNS Commander: per-provider DoH / UDP switching",
                    "Per-app filtering: only selected processes are bypassed",
                    "Gaming mode: optimised for Discord & Roblox latency",
                    "Allowlist: bypass DPI for trusted domains (All/Apps modes)",
                    "Inline Blacklist editor: manage SNI domains in-app",
                    "Protocol benchmark: TCP/HTTPS, UDP/DNS, QUIC score table",
                    "Auto-start on Windows boot via registry",
                    "Tray icon with quick start/stop/restart controls",
                    "Real-time console log viewer",
                    "",
                    "Developer: Gokhan Ozen",
                    "Website:  https://gman.dev",
                    "Project:  https://gman.dev/hayalet",
                };
                for (int i = 0; i < 15; i++)
                    SendMessage(hFeat, LB_ADDSTRING, 0, (LPARAM)features[i]);
            }

            SetDlgItemText(hwndDlg, IDC_BTN_TOGGLE, engine_running ? "Engine: ACTIVE" : "Engine: START");
            UpdateStatusText(hwndDlg);
            return TRUE;
        }
        case WM_NOTIFY: {
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            if (pnmhdr->idFrom == IDC_TAB_MAIN && pnmhdr->code == TCN_SELCHANGE) ShowTab(hwndDlg, TabCtrl_GetCurSel(GetDlgItem(hwndDlg, IDC_TAB_MAIN)));
            break;
        }
        case WM_TIMER: if (wParam == 1) { UpdateStatusText(hwndDlg); UpdateTrayIcon(); } if (wParam == 99) { KillTimer(hwndDlg, 99); SetDlgItemText(hwndDlg, IDOK, "Apply Settings"); } return TRUE;
        case WM_CTLCOLORDLG: return (INT_PTR)hbrBkgnd;
        case WM_CTLCOLORSTATIC:
            if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_TRAFFIC_GRAPH)) return (INT_PTR)hbrBkgnd;
            { SetTextColor((HDC)wParam, RGB(0, 0, 0)); SetBkMode((HDC)wParam, TRANSPARENT); return (INT_PTR)hbrBkgnd; }
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDC_LIST_DNS_PROVIDERS && HIWORD(wParam) == LBN_SELCHANGE) {
                int sel2 = (int)SendMessage(GetDlgItem(hwndDlg, IDC_LIST_DNS_PROVIDERS), LB_GETCURSEL, 0, 0);
                if (sel2 != LB_ERR && sel2 < 8) {
                    SetDlgItemText(hwndDlg, IDC_EDIT_DNS_IP, dns_table[sel2].ipv4);
                    SetDlgItemText(hwndDlg, IDC_EDIT_DNS_PORT, dns_table[sel2].port);
                    SetDlgItemText(hwndDlg, IDC_EDIT_CUSTOM_DOH, dns_table[sel2].doh_url);
                }
            }
            // --- Inline Allowlist handlers ---
            if (id == IDC_BTN_INLINE_AL_ADD) {
                char item[256]; GetDlgItemText(hwndDlg, IDC_EDIT_INLINE_AL, item, sizeof(item));
                if (strlen(item) > 0) { SendMessage(GetDlgItem(hwndDlg, IDC_LIST_INLINE_AL), LB_ADDSTRING, 0, (LPARAM)item); SetDlgItemText(hwndDlg, IDC_EDIT_INLINE_AL, ""); }
                return TRUE;
            }
            if (id == IDC_BTN_INLINE_AL_DEL) {
                HWND hL = GetDlgItem(hwndDlg, IDC_LIST_INLINE_AL);
                int sel = (int)SendMessage(hL, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) SendMessage(hL, LB_DELETESTRING, (WPARAM)sel, 0);
                return TRUE;
            }
            // --- Inline Blacklist handlers ---
            if (id == IDC_BTN_INLINE_BL_ADD) {
                char item[256]; GetDlgItemText(hwndDlg, IDC_EDIT_INLINE_BL, item, sizeof(item));
                if (strlen(item) > 0) { SendMessage(GetDlgItem(hwndDlg, IDC_LIST_INLINE_BL), LB_ADDSTRING, 0, (LPARAM)item); SetDlgItemText(hwndDlg, IDC_EDIT_INLINE_BL, ""); }
                return TRUE;
            }
            if (id == IDC_BTN_INLINE_BL_DEL) {
                HWND hL = GetDlgItem(hwndDlg, IDC_LIST_INLINE_BL);
                int sel = (int)SendMessage(hL, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) SendMessage(hL, LB_DELETESTRING, (WPARAM)sel, 0);
                return TRUE;
            }
            // --- Allowlist enable/disable on mode change ---
            if (id == IDC_RAD_ALL || id == IDC_RAD_BKL || id == IDC_RAD_APPS || id == IDC_RAD_GAMING) {
                BOOL al_ok = (id == IDC_RAD_ALL || id == IDC_RAD_APPS);
                EnableWindow(GetDlgItem(hwndDlg, IDC_LIST_INLINE_AL),    al_ok);
                EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_INLINE_AL),    al_ok);
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_INLINE_AL_ADD), al_ok);
                EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_INLINE_AL_DEL), al_ok);
            }
            // --- About URL buttons ---
            if (id == IDC_BTN_ABOUT_DEV)  ShellExecute(hwndDlg, "open", "https://gman.dev", NULL, NULL, SW_SHOWNORMAL);
            if (id == IDC_BTN_ABOUT_PROJ) ShellExecute(hwndDlg, "open", "https://gman.dev/hayalet", NULL, NULL, SW_SHOWNORMAL);
            if (id == IDC_BTN_TOGGLE) { ToggleEngine(); SetDlgItemText(hwndDlg, IDC_BTN_TOGGLE, engine_running ? "Engine: ACTIVE" : "Engine: START"); UpdateStatusText(hwndDlg); }
            if (id == IDC_BTN_APP_SELECTOR) DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_APP_SELECTOR), hwndDlg, AppSelectorDialogProc, 0);
            if (id == IDC_BTN_TEST) DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_TESTER), hwndDlg, TesterDialogProc, 0);
            if (id == IDOK) {
                const char* mode_vals[] = { "-1", "-2", "-3", "-4", "-5", "-6", "-7", "-8", "-9", "0" };
                char fmode[16], dns_ip[256], doh_url[512], dpreset[16];
                int sel = (int)SendMessage(GetDlgItem(hwndDlg, IDC_COMBO_MODE), CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 10) WritePrivateProfileString("Settings", "Mode", mode_vals[sel], ini_file);

                WritePrivateProfileString("Settings", "DoH", IsDlgButtonChecked(hwndDlg, IDC_RAD_DNS_SECURE) ? "1" : "0", ini_file);

                if      (IsDlgButtonChecked(hwndDlg, IDC_RAD_ALL))    strcpy(fmode, "0");
                else if (IsDlgButtonChecked(hwndDlg, IDC_RAD_BKL))    strcpy(fmode, "1");
                else if (IsDlgButtonChecked(hwndDlg, IDC_RAD_APPS))   strcpy(fmode, "2");
                else if (IsDlgButtonChecked(hwndDlg, IDC_RAD_GAMING)) strcpy(fmode, "3");
                else strcpy(fmode, "0");
                WritePrivateProfileString("Settings", "FilterMode", fmode, ini_file);

                // Allowlist enabled only if mode is ALL or APPS
                int al_active = (atoi(fmode) == 0 || atoi(fmode) == 2);
                WritePrivateProfileString("Settings", "AllowlistEnabled", al_active ? "1" : "0", ini_file);

                // SetAutoStart removed - now handled by Startup folder shortcut in installer.

                int psel = (int)SendMessage(GetDlgItem(hwndDlg, IDC_LIST_DNS_PROVIDERS), LB_GETCURSEL, 0, 0);
                sprintf(dpreset, "%d", psel); WritePrivateProfileString("Settings", "DNSPreset", dpreset, ini_file);

                char dns_port[16];
                GetDlgItemText(hwndDlg, IDC_EDIT_DNS_IP, dns_ip, 256); WritePrivateProfileString("Settings", "DNSAddr", dns_ip, ini_file);
                GetDlgItemText(hwndDlg, IDC_EDIT_DNS_PORT, dns_port, 16); WritePrivateProfileString("Settings", "DNSPort", dns_port, ini_file);
                GetDlgItemText(hwndDlg, IDC_EDIT_CUSTOM_DOH, doh_url, 512); WritePrivateProfileString("Settings", "DOHUrl", doh_url, ini_file);

                // Save inline lists
                {
                    HWND hAL = GetDlgItem(hwndDlg, IDC_LIST_INLINE_AL);
                    int cnt = (int)SendMessage(hAL, LB_GETCOUNT, 0, 0);
                    FILE *f = fopen(USER_FILES_PATH "allowlist.txt", "w");
                    if (f) { for (int i=0; i<cnt; i++) { char ln[256]; SendMessage(hAL, LB_GETTEXT, (WPARAM)i, (LPARAM)ln); fprintf(f, "%s\n", ln); } fclose(f); }
                }
                {
                    HWND hBL = GetDlgItem(hwndDlg, IDC_LIST_INLINE_BL);
                    int cnt = (int)SendMessage(hBL, LB_GETCOUNT, 0, 0);
                    FILE *f = fopen(USER_FILES_PATH "blacklist.txt", "w");
                    if (f) { for (int i=0; i<cnt; i++) { char ln[256]; SendMessage(hBL, LB_GETTEXT, (WPARAM)i, (LPARAM)ln); fprintf(f, "%s\n", ln); } fclose(f); }
                }

                SetDlgItemText(hwndDlg, IDOK, "Changes Applied!"); SetTimer(hwndDlg, 99, 2000, NULL); RestartEngine();
            }
            if (id == IDCANCEL) EndDialog(hwndDlg, IDCANCEL);
            break;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_TRAFFIC_GRAPH) {
                DrawTrafficGraph(lpDrawItem->hwndItem, lpDrawItem->hDC);
                return TRUE;
            }
            break;
        }
        case WM_CLOSE: EndDialog(hwndDlg, 0); return TRUE;
        case WM_DESTROY: KillTimer(hwndDlg, 1); break;
    }
    return FALSE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
                if (lParam == WM_RBUTTONUP) {
                    POINT pt; GetCursorPos(&pt); HMENU hMenu = CreatePopupMenu();
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_SETTINGS, "Hayalet Engine Settings");
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_TOGGLE, engine_running ? "Stop Hayalet" : "Start Hayalet");
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_RESTART, "Restart Hayalet");
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_VIEW_LOGS, "View Hayalet Logs");
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, "Exit");
                    SetForegroundWindow(hwnd); TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL); DestroyMenu(hMenu);
                } else { DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDialogProc, 0); }
            }
            break;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_EXIT: PostQuitMessage(0); break;
                case IDM_SETTINGS: DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDialogProc, 0); break;
                case IDM_RESTART: RestartEngine(); break;
                case IDM_TOGGLE: ToggleEngine(); break;
                case IDM_VIEW_LOGS: DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_LOG_VIEWER), hwnd, LogViewerDialogProc, 0); break;
            }
            break;
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Single Instance Guard: Prevent multiple Hayalet instances
    HANDLE hMutex = CreateMutex(NULL, TRUE, "HayaletDPI_GokhanOzen_UniqueMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0; // Already running, exit silently
    }

    freopen(log_file, "w", stdout); freopen(log_file, "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
    WSADATA wsaData; WSAStartup(MAKEWORD(2,2), &wsaData);
    hInst = hInstance; const char CLASS_NAME[] = "HayaletTrayClass";
    WNDCLASS wc = {0}; wc.lpfnWndProc = WindowProc; wc.hInstance = hInstance; wc.lpszClassName = CLASS_NAME; RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "Hayalet Tray", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    main_hwnd = hwnd; if (hwnd == NULL) return 0;
    nid.cbSize = sizeof(NOTIFYICONDATA); nid.hWnd = hwnd; nid.uID = ID_TRAYICON; nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, "id"); if (!nid.hIcon) nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); strcpy(nid.szTip, "Hayalet"); Shell_NotifyIcon(NIM_ADD, &nid);
    StartEngine(); // Silent startup to tray confirmed.
    MSG msg = {0}; while (GetMessage(&msg, NULL, 0, 0)) { if (!IsDialogMessage(hwnd, &msg)) { TranslateMessage(&msg); DispatchMessage(&msg); } }
    Shell_NotifyIcon(NIM_DELETE, &nid); StopEngine(); return 0;
}
