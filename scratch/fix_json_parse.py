import os
import re

html_path = r'c:\Users\mrander\Desktop\projectsnew\hayaletdpi\ui_v2\index.html'
with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Fix applyTranslations JSON.parse
trans_search = r"const transStr = await window\.v2_get_translation\(lang\);\n\s*if \(transStr && transStr !== '\{\}'\) \{\n\s*i18nData = JSON\.parse\(transStr\);"
trans_repl = r"const transStr = await window.v2_get_translation(lang);\n      if (transStr) {\n        i18nData = typeof transStr === 'string' ? JSON.parse(transStr) : transStr;"
content = re.sub(trans_search, trans_repl, content)

# 2. Fix cb_get_isp JSON.parse
isp_search = r"const data = JSON\.parse\(await window\.v2_get_isp\(\)\);"
isp_repl = r"let data = await window.v2_get_isp(); if (typeof data === 'string') data = JSON.parse(data);"
content = re.sub(isp_search, isp_repl, content)

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Fixed JSON.parse crashes in JS.")
