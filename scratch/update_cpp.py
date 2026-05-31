import os
import re

cpp_path = r'c:\Users\mrander\Desktop\projectsnew\hayaletdpi\src\webview_v2.cc'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Update cb_get_config to read Language
cb_get_config_search = r'char mode\[16\]=\{\}, dns_ip\[256\]=\{\}, dns_port\[16\]=\{\}, doh_url\[512\]=\{\}, filter\[16\]=\{\};'
cb_get_config_repl = r'char mode[16]={}, dns_ip[256]={}, dns_port[16]={}, doh_url[512]={}, filter[16]={}, lang[16]={};'
content = re.sub(cb_get_config_search, cb_get_config_repl, content)

content = content.replace('GetPrivateProfileString("Settings", "DOHUrl", "", doh_url, sizeof(doh_url), ini_file);',
                          'GetPrivateProfileString("Settings", "DOHUrl", "", doh_url, sizeof(doh_url), ini_file);\n    GetPrivateProfileString("Settings", "Language", "en", lang, sizeof(lang), ini_file);')

json_format_search = r'"\{\\"mode\\":\\"%s\\",\\"filter_mode\\":\\"%s\\",\\"dns_ip\\":\\"%s\\",\\"dns_port\\":\\"%s\\",\\"doh_url\\":\\"%s\\""'
json_format_repl = r'"{\"mode\":\"%s\",\"filter_mode\":\"%s\",\"dns_ip\":\"%s\",\"dns_port\":\"%s\",\"doh_url\":\"%s\",\"lang\":\"%s\"}"'
content = content.replace(json_format_search.replace('\\', ''), json_format_repl.replace('\\', ''))

content = content.replace('mode, filter, dns_ip, dns_port, doh_url);', 'mode, filter, dns_ip, dns_port, doh_url, lang);')

# 2. Update cb_save_config to write Language
cb_save_config_search = r'char m\[32\]=\{\}, fm\[32\]=\{\}, dip\[256\]=\{\}, dp\[32\]=\{\}, doh\[512\]=\{\};'
cb_save_config_repl = r'char m[32]={}, fm[32]={}, dip[256]={}, dp[32]={}, doh[512]={}, lang[32]={};'
content = re.sub(cb_save_config_search, cb_save_config_repl, content)

extract_search = r'extract_json_string\(req, 4, doh, sizeof\(doh\)\);'
extract_repl = r'extract_json_string(req, 4, doh, sizeof(doh));\n    extract_json_string(req, 5, lang, sizeof(lang));'
content = re.sub(extract_search, extract_repl, content)

write_search = r'WritePrivateProfileString\("Settings", "DOHUrl", doh, ini_file\);'
write_repl = r'WritePrivateProfileString("Settings", "DOHUrl", doh, ini_file);\n    WritePrivateProfileString("Settings", "Language", lang, ini_file);'
content = re.sub(write_search, write_repl, content)

# 3. Add cb_get_translation
translation_func = """
static void cb_get_translation(const char *seq, const char *req, void *arg) {
    char lang[32]={};
    extract_json_string(req, 0, lang, sizeof(lang));
    
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    char *lastBs = strrchr(exePath, '\\\\');
    if (lastBs) *lastBs = '\\0';
    
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\\\ui_v2\\\\lang\\\\%s.json", exePath, lang);
    if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
        snprintf(path, sizeof(path), "%s\\\\..\\\\..\\\\ui_v2\\\\lang\\\\%s.json", exePath, lang);
    }
    
    FILE* f = fopen(path, "r");
    if (!f) {
        webview_return((webview_t)arg, seq, 0, "{}");
        return;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 1);
    size_t r = fread(buf, 1, size, f); buf[r] = 0; fclose(f);
    
    webview_return((webview_t)arg, seq, 0, buf);
    free(buf);
}
"""
content = content.replace('static void cb_get_isp', translation_func + '\nstatic void cb_get_isp')

# 4. Bind cb_get_translation
bind_search = r'webview_bind\(w, "v2_get_isp",       cb_get_isp,       w\);'
bind_repl = r'webview_bind(w, "v2_get_isp",       cb_get_isp,       w);\n    webview_bind(w, "v2_get_translation", cb_get_translation, w);'
content = re.sub(bind_search, bind_repl, content)


with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("webview_v2.cc updated successfully.")
