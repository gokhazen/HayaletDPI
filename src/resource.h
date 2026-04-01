// ===================================================
// Hayalet Engine Resource IDs
// ===================================================

// -- Dialog IDs --
#define IDD_SETTINGS          101
#define IDD_APP_SELECTOR      102
#define IDD_TESTER            103
#define IDD_LOG_VIEWER        104
#define IDD_ALLOWLIST         105
#define IDD_BLACKLIST_EDITOR  106

// -- Tab & top-level controls --
#define IDC_TAB_MAIN        1020
#define IDC_STATUS_TEXT     1011
#define IDC_BTN_TOGGLE      1010
#define IDC_CHK_AUTOSTART   1012

// -- Tab 1: Dashboard --
#define IDC_GRP_DASHBOARD           1201
#define IDC_LBL_PROCESSED           1301
#define IDC_LBL_BYPASS              1302
#define IDC_LBL_RATIO               1303
#define IDC_LBL_TRAFFIC_FLOW        1304
#define IDC_TRAFFIC_PROCESSED       1305
#define IDC_TRAFFIC_CIRCUMVENTED    1306
#define IDC_TRAFFIC_RATIO           1307
#define IDC_TRAFFIC_GRAPH           1308

// -- Tab 2: Engine --
#define IDC_GRP_ENGINE          1202
#define IDC_LBL_STEALTH         1401
#define IDC_COMBO_MODE          1001
#define IDC_LBL_DNS_HEADER      1402
#define IDC_LBL_SELECT_PROV     1403
#define IDC_LIST_DNS_PROVIDERS  1002
#define IDC_LBL_PROTO_TYPE      1404
#define IDC_RAD_DNS_STANDARD    1003
#define IDC_RAD_DNS_SECURE      1004
#define IDC_LBL_PROV_IP         1405
#define IDC_EDIT_DNS_IP         1005
#define IDC_LBL_DNS_PORT        1406
#define IDC_EDIT_DNS_PORT       1022
#define IDC_LBL_DOH_URL         1407
#define IDC_EDIT_CUSTOM_DOH     1006
#define IDC_LBL_DNS_STATUS      1007
#define IDC_BTN_TEST            1013

// -- Tab 3: Filter --
#define IDC_GRP_DNS             1203
#define IDC_RAD_ALL             1016
#define IDC_RAD_BKL             1017
#define IDC_RAD_APPS            1018
#define IDC_RAD_GAMING          1019
#define IDC_EDIT_BLACKLIST      1008
#define IDC_BTN_EDIT_BKL        1014
#define IDC_BTN_APP_SELECTOR    1015
#define IDC_LBL_GAMETIP         1409
#define IDC_SEP_FILTER          2030   /* named so ShowTab can hide it */
// Inline Allowlist editor (left column)
#define IDC_LBL_ALLOWLIST_INFO  1410
#define IDC_LIST_INLINE_AL      2001
#define IDC_EDIT_INLINE_AL      2002
#define IDC_BTN_INLINE_AL_ADD   2003
#define IDC_BTN_INLINE_AL_DEL   2004
// Inline Blacklist editor (right column)
#define IDC_LBL_INLINE_BL       2011
#define IDC_LIST_INLINE_BL      2012
#define IDC_EDIT_INLINE_BL      2013
#define IDC_BTN_INLINE_BL_ADD   2014
#define IDC_BTN_INLINE_BL_DEL   2015

// -- Tab 4: About --
#define IDC_GRP_ADVANCED          1204
#define IDC_LBL_CURBUILD          1411
#define IDC_LBL_ABOUT_DEV         1413
#define IDC_BTN_ABOUT_DEV         2020  /* opens https://gman.dev */
#define IDC_BTN_ABOUT_PROJ        2021  /* opens https://gman.dev/hayalet */
#define IDC_SEP_ABOUT             2031  /* named so ShowTab can hide it */
#define IDC_LBL_ABOUT_FEATURES    2032
#define IDC_LIST_FEATURES         2022
// (IDC_EDIT_RELEASE_NOTES kept for compat but no longer in main dialog)
#define IDC_EDIT_RELEASE_NOTES  1412

// -- App Selector Dialog --
#define IDC_LIST_APPS           1501
#define IDC_BTN_REFRESH_APPS    1502

// -- Tester Dialog --
#define IDC_LIST_TEST           1601
#define IDC_EDIT_TEST_URL       1602
#define IDC_BTN_START_TEST      1603

// -- Log Viewer Dialog --
#define IDC_EDIT_LOG_LIVE       1701
#define IDC_BTN_CLEAR_LOG       1702

// -- Allowlist Dialog --
#define IDC_LIST_ALLOWLIST      1801
#define IDC_EDIT_ALLOWLIST_ITEM 1802
#define IDC_BTN_ALLOWLIST_ADD   1803
#define IDC_BTN_ALLOWLIST_DEL   1804
#define IDC_BTN_ALLOWLIST_SAVE  1805

// -- Blacklist Editor Dialog --
#define IDC_LIST_BL_EDITOR      1901
#define IDC_EDIT_BL_ITEM        1902
#define IDC_BTN_BL_ADD          1903
#define IDC_BTN_BL_DEL          1904
#define IDC_BTN_BL_SAVE         1905

// -- Tray Menu --
#define IDM_SETTINGS    2001
#define IDM_EXIT        2002
#define IDM_TOGGLE      2003
#define IDM_RESTART     2004
#define IDM_VIEW_LOGS   2005

// -- Unused legacy (kept for compat) --
#define IDC_BTN_SMART_MODE  1023
#define IDC_CHK_DOH         1021
