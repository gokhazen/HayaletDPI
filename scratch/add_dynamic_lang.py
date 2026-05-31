import os
import re

html_path = r'c:\Users\mrander\Desktop\projectsnew\hayaletdpi\ui_v2\index.html'
with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Update the onchange handler
content = content.replace('<select id="cfg-lang" onchange="dirty()">', '<select id="cfg-lang" onchange="dirty(); changeLanguage();">')

# 2. Add the changeLanguage function inside the JS block
change_lang_code = """
async function changeLanguage() {
    const newLang = document.getElementById('cfg-lang').value;
    if (newLang !== currentLang) {
        currentLang = newLang;
        await applyTranslations(currentLang);
        const activeView = document.querySelector('.view.active');
        if (activeView) {
            nav(activeView.id.replace('view-', ''));
        }
    }
}
"""
content = content.replace('let currentLang = \'en\';', change_lang_code + '\nlet currentLang = \'en\';')

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Dynamic language change added successfully.")
