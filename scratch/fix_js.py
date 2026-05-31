import os
import re

html_path = r'c:\Users\mrander\Desktop\projectsnew\hayaletdpi\ui_v2\index.html'
with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the old i18n block
old_i18n_pattern = re.compile(r'// ── i18n ──.*?setTimeout\(initLanguage, 100\);\n', re.DOTALL)
content = old_i18n_pattern.sub('', content)

# Fix nav() locales usage
nav_search = r"const dict = locales\[currentLang\] \|\| locales\['english'\];\n  const titleMap = \{'stats':'nav_stats','engine':'nav_engine','filters':'nav_policy','tester':'nav_diag','logs':'nav_logs','update':'nav_update','about':'nav_about'\};\n  document.getElementById\('page-title'\)\.textContent = \(dict && dict\[titleMap\[id\]\]\) \? dict\[titleMap\[id\]\] : \(TITLES\[id\]\|\|id\);"
nav_repl = r"const dict = i18nData;\n  const titleMap = {'stats':'nav_telemetrics','engine':'nav_engine','filters':'nav_policy','tester':'nav_diag','logs':'nav_logs','update':'nav_update','about':'nav_about'};\n  document.getElementById('page-title').textContent = (dict && dict[titleMap[id]]) ? dict[titleMap[id]] : (TITLES[id]||id);"
content = re.sub(nav_search, nav_repl, content)

# Fix setEngineUI locales usage
seteng_search = r"document\.getElementById\('eng-label'\)\.textContent=on\?\(\(locales\[currentLang\]&&locales\[currentLang\]\['status_running'\]\)\|\|'Running'\):\(\(locales\[currentLang\]&&locales\[currentLang\]\['status_stopped'\]\)\|\|'Stopped'\);\n  document\.getElementById\('eng-sub'\)\.textContent=on\?\(\(locales\[currentLang\]&&locales\[currentLang\]\['status_online'\]\)\|\|'Online'\):\(\(locales\[currentLang\]&&locales\[currentLang\]\['status_offline'\]\)\|\|'Offline'\);"
seteng_repl = r"document.getElementById('eng-label').textContent = on ? (i18nData['engine_running'] || 'RUNNING') : (i18nData['engine_stopped'] || 'STOPPED');\n  document.getElementById('eng-sub').textContent = on ? (i18nData['engine_online'] || 'ONLINE') : (i18nData['engine_offline'] || 'OFFLINE');"
content = re.sub(seteng_search, seteng_repl, content)

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("JS fixed successfully.")
