import os
import re

html_path = r'c:\Users\mrander\Desktop\projectsnew\hayaletdpi\ui_v2\index.html'
with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace texts with data-i18n spans or attributes
replacements = {
    r'<button class="sb-item active" id="sb-stats" onclick="nav\(\'stats\'\)">TELEMETRICS</button>': r'<button class="sb-item active" id="sb-stats" onclick="nav(\'stats\')" data-i18n="nav_telemetrics">TELEMETRICS</button>',
    r'<button class="sb-item" id="sb-engine" onclick="nav\(\'engine\'\)">ENGINE CORE</button>': r'<button class="sb-item" id="sb-engine" onclick="nav(\'engine\')" data-i18n="nav_engine">ENGINE CORE</button>',
    r'<button class="sb-item" id="sb-filters" onclick="nav\(\'filters\'\)">ACCESS POLICY</button>': r'<button class="sb-item" id="sb-filters" onclick="nav(\'filters\')" data-i18n="nav_policy">ACCESS POLICY</button>',
    r'<button class="sb-item" id="sb-tester" onclick="nav\(\'tester\'\)">DIAGNOSTICS</button>': r'<button class="sb-item" id="sb-tester" onclick="nav(\'tester\')" data-i18n="nav_diag">DIAGNOSTICS</button>',
    r'<button class="sb-item" id="sb-logs" onclick="nav\(\'logs\'\)">LIVE FEED</button>': r'<button class="sb-item" id="sb-logs" onclick="nav(\'logs\')" data-i18n="nav_logs">LIVE FEED</button>',
    r'<button class="sb-item" id="sb-update" onclick="nav\(\'update\'\)">UPDATES</button>': r'<button class="sb-item" id="sb-update" onclick="nav(\'update\')" data-i18n="nav_update">UPDATES</button>',
    r'<button class="sb-item" id="sb-about" onclick="nav\(\'about\'\)">ABOUT</button>': r'<button class="sb-item" id="sb-about" onclick="nav(\'about\')" data-i18n="nav_about">ABOUT</button>',
    
    r'<div class="stat-label">Ingress Analyzed</div>': r'<div class="stat-label" data-i18n="stat_ingress">Ingress Analyzed</div>',
    r'<div class="stat-label">Stealth Bypassed</div>': r'<div class="stat-label" data-i18n="stat_stealth">Stealth Bypassed</div>',
    r'<div class="stat-label">Efficiency Ratio</div>': r'<div class="stat-label" data-i18n="stat_ratio">Efficiency Ratio</div>',
    r'<div class="stat-label">Live Throughput</div>': r'<div class="stat-label" data-i18n="stat_throughput">Live Throughput</div>',
    r'<span>Throughput Telemetry</span>': r'<span data-i18n="card_throughput">Throughput Telemetry</span>',
    r'<div class="card-title">Network & System Status</div>': r'<div class="card-title" data-i18n="card_network">Network & System Status</div>',
    r'<span class="fl" style="margin:0;">ISP / Provider</span>': r'<span class="fl" style="margin:0;" data-i18n="lbl_isp">ISP / Provider</span>',
    r'<span class="fl" style="margin:0;">Engine State</span>': r'<span class="fl" style="margin:0;" data-i18n="lbl_engine_state">Engine State</span>',
    r'<div class="card-title">Active DNS Route</div>': r'<div class="card-title" data-i18n="card_dns">Active DNS Route</div>',
    r'<span class="fl" style="margin:0;">Resolution Stack</span>': r'<span class="fl" style="margin:0;" data-i18n="lbl_res_stack">Resolution Stack</span>',
    r'<span class="fl" style="margin:0;">Resolver Target</span>': r'<span class="fl" style="margin:0;" data-i18n="lbl_res_target">Resolver Target</span>',
    r'<div class="card-title">Stealth Configuration</div>': r'<div class="card-title" data-i18n="card_stealth_cfg">Stealth Configuration</div>',
    r'<label class="fl">Protocol Algorithm</label>': r'<label class="fl" data-i18n="lbl_protocol">Protocol Algorithm</label>',
    r'<label class="fl">Traffic Scope</label>': r'<label class="fl" data-i18n="lbl_traffic_scope">Traffic Scope</label>',
    r'<label class="fl">System Startup</label>': r'<label class="fl" data-i18n="lbl_sys_startup">System Startup</label>',
    r'Run at Login': r'<span data-i18n="lbl_run_login">Run at Login</span>',
    r'<div class="card-title">DNS Resolution</div>': r'<div class="card-title" data-i18n="card_dns_res">DNS Resolution</div>',
    r'<button id="btn-dns-preset" class="dns-type-btn active" onclick="setDnsType\(\'preset\'\)">Presets</button>': r'<button id="btn-dns-preset" class="dns-type-btn active" onclick="setDnsType(\'preset\')" data-i18n="btn_presets">Presets</button>',
    r'<button id="btn-dns-custom" class="dns-type-btn" onclick="setDnsType\(\'custom\'\)">Custom</button>': r'<button id="btn-dns-custom" class="dns-type-btn" onclick="setDnsType(\'custom\')" data-i18n="btn_custom">Custom</button>',
    r'<label class="fl">Provider</label>': r'<label class="fl" data-i18n="lbl_provider">Provider</label>',
    r'<label class="fl">IP Address</label>': r'<label class="fl" data-i18n="lbl_ip">IP Address</label>',
    r'<label class="fl" id="grp-port" style="flex:1">Port</label>': r'<label class="fl" id="grp-port" style="flex:1" data-i18n="lbl_port">Port</label>',
    r'<label class="fl">DoH URL</label>': r'<label class="fl" data-i18n="lbl_doh">DoH URL</label>',
    r'<div class="card-title">Blacklist <span id="cnt-blacklist" class="badge-count">0</span></div>': r'<div class="card-title"><span data-i18n="card_bl">Blacklist</span> <span id="cnt-blacklist" class="badge-count">0</span></div>',
    r'<div class="card-title">Allowlist <span id="cnt-allowlist" class="badge-count">0</span></div>': r'<div class="card-title"><span data-i18n="card_al">Allowlist</span> <span id="cnt-allowlist" class="badge-count">0</span></div>',
    r'<button class="btn" onclick="addItem\(\'blacklist\'\)">ADD</button>': r'<button class="btn" onclick="addItem(\'blacklist\')" data-i18n="btn_add">ADD</button>',
    r'<button class="btn" onclick="clearList\(\'blacklist\'\)">CLEAR</button>': r'<button class="btn" onclick="clearList(\'blacklist\')" data-i18n="btn_clear">CLEAR</button>',
    r'<button class="btn" onclick="addItem\(\'allowlist\'\)">ADD</button>': r'<button class="btn" onclick="addItem(\'allowlist\')" data-i18n="btn_add">ADD</button>',
    r'<button class="btn" onclick="clearList\(\'allowlist\'\)">CLEAR</button>': r'<button class="btn" onclick="clearList(\'allowlist\')" data-i18n="btn_clear">CLEAR</button>',
    r'<span>Target Applications</span>': r'<span data-i18n="card_apps">Target Applications</span>',
    r'<button class="btn btn-primary" onclick="openAppModal\(\)">MANAGE</button>': r'<button class="btn btn-primary" onclick="openAppModal()" data-i18n="btn_manage">MANAGE</button>',
    r'<span>Matrix Diagnostic \(<span id="combo-count">0</span> Combos\)</span>': r'<span data-i18n="card_matrix">Matrix Diagnostic</span> (<span id="combo-count">0</span>)',
    r'<button class="btn btn-primary" id="btn-probe" onclick="runProbe\(\)">RUN PROBE</button>': r'<button class="btn btn-primary" id="btn-probe" onclick="runProbe()" data-i18n="btn_run_probe">RUN PROBE</button>',
    r'<button class="btn" id="btn-stop" onclick="stopProbe\(\)" style="display:none;">STOP</button>': r'<button class="btn" id="btn-stop" onclick="stopProbe()" style="display:none;" data-i18n="btn_stop">STOP</button>',
    r'<span>Engine Log</span>': r'<span data-i18n="card_log">Engine Log</span>',
    r'<button class="btn btn-primary" onclick="checkUpdate\(\)">CHECK FOR UPDATES</button>': r'<button class="btn btn-primary" onclick="checkUpdate()" data-i18n="btn_check_upd">CHECK FOR UPDATES</button>',
    r'<button class="btn" onclick="openLink\(\'https://github.com/gokhazen/HayaletDPI/releases\'\)">RELEASES PAGE</button>': r'<button class="btn" onclick="openLink(\'https://github.com/gokhazen/HayaletDPI/releases\')" data-i18n="btn_releases">RELEASES PAGE</button>',
    r'<p>UNSAVED CHANGES</p>': r'<p data-i18n="txt_unsaved">UNSAVED CHANGES</p>',
    r'<button class="btn" style="border:1px solid #000;" onclick="discardCfg\(\)">DISCARD</button>': r'<button class="btn" style="border:1px solid #000;" onclick="discardCfg()" data-i18n="btn_discard">DISCARD</button>',
    r'<button class="btn btn-primary" style="background:#000;color:var\(--accent\);" onclick="saveConfig\(\)">APPLY & RESTART</button>': r'<button class="btn btn-primary" style="background:#000;color:var(--accent);" onclick="saveConfig()" data-i18n="btn_apply">APPLY & RESTART</button>',
    r'<h3>Target Applications</h3>': r'<h3 data-i18n="modal_apps_title">Target Applications</h3>',
    r'<button class="btn" onclick="closeAppModal\(\)">Cancel</button>': r'<button class="btn" onclick="closeAppModal()" data-i18n="btn_cancel">Cancel</button>',
    r'<button class="btn btn-primary" onclick="confirmApps\(\)">Apply</button>': r'<button class="btn btn-primary" onclick="confirmApps()" data-i18n="btn_apply_modal">Apply</button>',
    r'<option value="0">Global</option>': r'<option value="0" data-i18n="scope_global">Global</option>',
    r'<option value="1">Blacklist</option>': r'<option value="1" data-i18n="scope_bl">Blacklist</option>',
    r'<option value="2">App Target</option>': r'<option value="2" data-i18n="scope_app">App Target</option>',
    r'<option value="3">Gaming</option>': r'<option value="3" data-i18n="scope_game">Gaming</option>',
    r'<option value="std">Standard UDP</option>': r'<option value="std" data-i18n="dns_std">Standard UDP</option>',
    r'<option value="doh">DNS-over-HTTPS</option>': r'<option value="doh" data-i18n="dns_doh">DNS-over-HTTPS</option>',
}

for src, dst in replacements.items():
    content = re.sub(src, dst, content)

# Add language selector in view-engine
lang_selector = """
          <div class="fg" style="margin-top: 32px; border-top: 1px solid var(--border); padding-top: 16px;">
            <label class="fl" data-i18n="lbl_lang">Language / Dil</label>
            <select id="cfg-lang" onchange="dirty()">
              <option value="en">English</option>
              <option value="tr">Türkçe</option>
            </select>
          </div>
"""
content = content.replace('</label>\n          </div>\n        </div>', '</label>\n          </div>' + lang_selector + '\n        </div>')

# Add translation logic in script
translation_js = """
let currentLang = 'en';
let i18nData = {};

async function applyTranslations(lang) {
  if (window.v2_get_translation) {
    try {
      const transStr = await window.v2_get_translation(lang);
      if (transStr && transStr !== '{}') {
        i18nData = JSON.parse(transStr);
        document.querySelectorAll('[data-i18n]').forEach(el => {
          const key = el.getAttribute('data-i18n');
          if (i18nData[key]) {
            if (el.tagName === 'INPUT' && el.type === 'button') {
                el.value = i18nData[key];
            } else if (el.tagName === 'OPTION') {
                el.textContent = i18nData[key];
            } else {
                el.textContent = i18nData[key];
            }
          }
        });
      }
    } catch(e) {}
  }
}
"""
content = content.replace('// ── AI OPTIMIZATION LOGIC ───────────────────────────────────────────────────────', translation_js + '\n// ── AI OPTIMIZATION LOGIC ───────────────────────────────────────────────────────')

# Update loadCfg
loadcfg_search = r'document\.getElementById\(\'cfg-dohurl\'\)\.value=c\.doh_url\|\|\'\';'
loadcfg_repl = r'document.getElementById(\'cfg-dohurl\').value=c.doh_url||\'\';\n    document.getElementById(\'cfg-lang\').value=c.lang||\'en\';\n    currentLang = c.lang||\'en\';\n    await applyTranslations(currentLang);'
content = re.sub(loadcfg_search, loadcfg_repl, content)

# Update saveConfig
savecfg_search = r'const dp=document\.getElementById\(\'cfg-dnsport\'\)\.value;'
savecfg_repl = r'const dp=document.getElementById(\'cfg-dnsport\').value;\n  const lang=document.getElementById(\'cfg-lang\').value;'
content = re.sub(savecfg_search, savecfg_repl, content)

savecfg_call_search = r'await window\.v2_save_config\(m,fm,dip,dp,doh\);'
savecfg_call_repl = r'await window.v2_save_config(m,fm,dip,dp,doh,lang);'
content = re.sub(savecfg_call_search, savecfg_call_repl, content)

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("HTML i18n applied successfully.")
