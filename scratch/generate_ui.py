import os
import re

html_path = r'c:\Users\mrander\Desktop\projectsnew\hayaletdpi\ui_v2\index.html'
with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Extract script
script_match = re.search(r'<script>(.*?)</script>\s*</body>', content, re.DOTALL)
if script_match:
    js_code = script_match.group(1).strip()
else:
    print("Could not find script block")
    exit(1)

new_html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HayaletDPI - Zero</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=JetBrains+Mono:wght@400;700&display=swap" rel="stylesheet">
<style>
:root {{
  --bg-main: #050505;
  --bg-panel: #0d0d0d;
  --bg-hover: #1a1a1a;
  --border: #222;
  --border-focus: #444;
  --text-main: #ededed;
  --text-muted: #888;
  --accent: #10b981;
  --danger: #ef4444;
  --warning: #f59e0b;
  --info: #3b82f6;
  --sidebar-w: 220px;
}}
* {{ margin: 0; padding: 0; box-sizing: border-box; }}
body {{ font-family: 'Inter', sans-serif; background: var(--bg-main); color: var(--text-main); height: 100vh; display: flex; overflow: hidden; font-size: 13px; }}

::-webkit-scrollbar {{ width: 0px; height: 0px; }}

.sidebar {{ width: var(--sidebar-w); background: var(--bg-panel); border-right: 1px solid var(--border); display: flex; flex-direction: column; flex-shrink: 0; }}
.sb-logo {{ padding: 20px; border-bottom: 1px solid var(--border); }}
.sb-logo h1 {{ font-size: 16px; font-weight: 700; letter-spacing: 1px; color: #fff; }}
.sb-logo p {{ font-size: 10px; color: var(--accent); text-transform: uppercase; letter-spacing: 2px; margin-top: 4px; }}

.sb-nav {{ flex: 1; padding: 10px; display: flex; flex-direction: column; gap: 4px; overflow-y: auto; }}
.sb-item {{ padding: 10px 14px; background: transparent; border: none; color: var(--text-muted); font-size: 12px; font-weight: 600; text-align: left; cursor: pointer; }}
.sb-item:hover {{ color: var(--text-main); background: var(--bg-hover); }}
.sb-item.active {{ color: #000; background: var(--text-main); }}

.sb-status {{ padding: 15px; border-top: 1px solid var(--border); }}
.engine-pill {{ background: var(--bg-main); border: 1px solid var(--border); padding: 12px; display: flex; align-items: center; justify-content: space-between; }}
.engine-info p {{ font-size: 12px; font-weight: 700; color: var(--text-main); }}
.engine-info span {{ font-size: 10px; color: var(--text-muted); font-family: 'JetBrains Mono', monospace; }}
.engine-dot {{ width: 8px; height: 8px; background: var(--danger); border-radius: 50%; }}
.engine-dot.on {{ background: var(--accent); }}
.engine-toggle {{ width: 24px; height: 24px; background: var(--border); border: none; color: var(--text-main); cursor: pointer; }}
.engine-toggle:hover {{ background: var(--border-focus); }}

.main {{ flex: 1; display: flex; flex-direction: column; min-width: 0; }}
.topbar {{ padding: 16px 24px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; background: var(--bg-main); }}
.page-title {{ font-size: 14px; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; }}
.topbar-right {{ display: flex; gap: 10px; }}
.badge {{ padding: 4px 8px; border: 1px solid var(--border); font-size: 10px; font-weight: 700; text-transform: uppercase; color: var(--text-muted); }}

.content {{ flex: 1; overflow-y: auto; padding: 24px; }}
.view {{ display: none; }}
.view.active {{ display: block; }}

.card {{ background: var(--bg-panel); border: 1px solid var(--border); padding: 20px; margin-bottom: 16px; }}
.card-title {{ font-size: 12px; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; color: var(--text-main); margin-bottom: 16px; border-bottom: 1px solid var(--border); padding-bottom: 8px; }}

.grid2 {{ display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }}
.grid4 {{ display: grid; grid-template-columns: repeat(4, 1fr); gap: 16px; }}

.stat-val {{ font-size: 28px; font-weight: 700; font-family: 'JetBrains Mono', monospace; color: #fff; margin-bottom: 4px; }}
.stat-label {{ font-size: 10px; font-weight: 600; text-transform: uppercase; color: var(--text-muted); }}

.fg {{ margin-bottom: 16px; }}
.fl {{ display: block; font-size: 10px; font-weight: 700; text-transform: uppercase; color: var(--text-muted); margin-bottom: 8px; }}
select, input[type=text] {{ width: 100%; background: var(--bg-main); border: 1px solid var(--border); padding: 10px; color: var(--text-main); font-family: 'JetBrains Mono', monospace; font-size: 12px; outline: none; }}
select:focus, input:focus {{ border-color: var(--accent); }}
input[type=checkbox] {{ appearance: none; width: 16px; height: 16px; background: var(--bg-main); border: 1px solid var(--border); cursor: pointer; }}
input[type=checkbox]:checked {{ background: var(--accent); border-color: var(--accent); }}

.btn {{ display: inline-block; padding: 8px 16px; font-family: 'Inter'; font-size: 11px; font-weight: 700; text-transform: uppercase; cursor: pointer; border: 1px solid var(--border); background: var(--bg-panel); color: var(--text-main); }}
.btn:hover {{ background: var(--bg-hover); }}
.btn-primary {{ background: var(--accent); color: #000; border-color: var(--accent); }}
.btn-primary:hover {{ background: #0e9f6e; }}

.tbl {{ width: 100%; border-collapse: collapse; }}
.tbl th {{ text-align: left; padding: 10px; font-size: 10px; font-weight: 700; text-transform: uppercase; color: var(--text-muted); border-bottom: 1px solid var(--border); }}
.tbl td {{ padding: 10px; font-family: 'JetBrains Mono', monospace; font-size: 11px; border-bottom: 1px solid var(--border); }}

.list-wrap {{ border: 1px solid var(--border); height: 200px; overflow-y: auto; background: var(--bg-main); }}
.list-item {{ display: flex; justify-content: space-between; padding: 8px 12px; border-bottom: 1px solid var(--border); }}
.list-item .name {{ font-family: 'JetBrains Mono', monospace; font-size: 11px; }}
.list-item .del {{ color: var(--danger); cursor: pointer; font-size: 14px; font-weight: 700; }}

.action-bar {{ position: fixed; bottom: 0; left: var(--sidebar-w); right: 0; background: var(--accent); color: #000; padding: 12px 24px; display: none; justify-content: space-between; align-items: center; border-top: 1px solid var(--border); z-index: 100; }}
.action-bar.show {{ display: flex; }}
.action-bar p {{ font-size: 12px; font-weight: 700; text-transform: uppercase; }}

.modal-bg {{ position: fixed; inset: 0; background: rgba(0,0,0,0.85); display: none; align-items: center; justify-content: center; z-index: 500; }}
.modal-bg.open {{ display: flex; }}
.modal {{ background: var(--bg-panel); border: 1px solid var(--border); width: 500px; max-height: 80vh; display: flex; flex-direction: column; }}
.modal-top {{ padding: 16px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; }}
.modal-top h3 {{ font-size: 14px; font-weight: 700; text-transform: uppercase; }}
.proc-list {{ flex: 1; overflow-y: auto; padding: 16px; }}
.proc-item {{ display: flex; align-items: center; gap: 12px; padding: 8px; border-bottom: 1px solid var(--border); cursor: pointer; }}
.proc-item:hover {{ background: var(--bg-hover); }}
.proc-item.sel {{ background: var(--text-main); color: #000; }}
.proc-item.sel .proc-name {{ color: #000; }}
.proc-name {{ font-family: 'JetBrains Mono', monospace; font-size: 12px; }}
.modal-footer {{ padding: 16px; border-top: 1px solid var(--border); display: flex; justify-content: flex-end; gap: 10px; }}

.dns-type-selector {{ display: flex; border: 1px solid var(--border); margin-bottom: 16px; }}
.dns-type-btn {{ flex: 1; padding: 10px; background: transparent; border: none; border-right: 1px solid var(--border); color: var(--text-muted); font-size: 11px; font-weight: 700; text-transform: uppercase; cursor: pointer; }}
.dns-type-btn:last-child {{ border-right: none; }}
.dns-type-btn.active {{ background: var(--text-main); color: #000; }}

/* Chart override for simplicity */
#chart {{ display: flex; align-items: flex-end; height: 60px; gap: 2px; margin-top: 10px; }}
.bar {{ flex: 1; background: var(--accent); }}

.oracle-box {{ border: 1px solid var(--accent); padding: 16px; margin-top: 16px; display: none; background: var(--bg-hover); justify-content: space-between; align-items: center; }}
.oracle-box.show {{ display: flex; }}
.oracle-txt {{ font-family: 'JetBrains Mono', monospace; font-size: 12px; }}

.t-test {{ color: var(--warning); }}
.t-ok {{ color: var(--accent); }}
.t-fail {{ color: var(--danger); }}

.badge-count {{ float: right; background: var(--border); padding: 2px 6px; font-size: 10px; }}
</style>
</head>
<body>

<div class="modal-bg" id="app-modal">
  <div class="modal">
    <div class="modal-top">
      <h3>Target Applications</h3>
      <button class="btn" onclick="closeAppModal()">X</button>
    </div>
    <div style="padding: 16px; border-bottom: 1px solid var(--border);">
      <input type="text" id="proc-search" placeholder="Filter processes..." oninput="filterProcs()">
    </div>
    <div class="proc-list" id="proc-list"></div>
    <div class="modal-footer">
      <span id="sel-count" style="margin-right: auto; line-height: 32px; font-size: 12px; font-weight: 700; color: var(--text-muted);">0 selected</span>
      <button class="btn" onclick="closeAppModal()">Cancel</button>
      <button class="btn btn-primary" onclick="confirmApps()">Apply</button>
    </div>
  </div>
</div>

<nav class="sidebar">
  <div class="sb-logo">
    <h1>HAYALET_DPI</h1>
    <p>Zero Edition</p>
  </div>
  <div class="sb-nav">
    <button class="sb-item active" id="sb-stats" onclick="nav('stats')">TELEMETRICS</button>
    <button class="sb-item" id="sb-engine" onclick="nav('engine')">ENGINE CORE</button>
    <button class="sb-item" id="sb-filters" onclick="nav('filters')">ACCESS POLICY</button>
    <button class="sb-item" id="sb-tester" onclick="nav('tester')">DIAGNOSTICS</button>
    <button class="sb-item" id="sb-logs" onclick="nav('logs')">LIVE FEED</button>
    <button class="sb-item" id="sb-update" onclick="nav('update')">UPDATES</button>
    <button class="sb-item" id="sb-about" onclick="nav('about')">ABOUT</button>
  </div>
  <div class="sb-status">
    <div class="engine-pill">
      <div id="eng-dot" class="engine-dot"></div>
      <div class="engine-info">
        <p id="eng-label">STOPPED</p>
        <span id="eng-sub">OFFLINE</span>
      </div>
      <button class="engine-toggle" id="eng-btn" onclick="toggleEngine()">PWR</button>
    </div>
  </div>
</nav>

<div class="main">
  <div class="topbar">
    <div class="page-title" id="page-title">TELEMETRICS</div>
    <div class="topbar-right">
      <div class="badge" id="top-mode-badge">MODE -5</div>
      <div class="badge" id="top-status-badge">OFFLINE</div>
    </div>
  </div>

  <div class="content">

    <!-- TELEMETRICS -->
    <div id="view-stats" class="view active">
      <div class="grid4" style="margin-bottom: 24px;">
        <div><div class="stat-val" id="stat-proc">0</div><div class="stat-label">Ingress Analyzed</div></div>
        <div><div class="stat-val" id="stat-circ" style="color: var(--accent);">0</div><div class="stat-label">Stealth Bypassed</div></div>
        <div><div class="stat-val" id="stat-ratio">0%</div><div class="stat-label">Efficiency Ratio</div></div>
        <div><div class="stat-val" id="chart-rate">0/s</div><div class="stat-label">Live Throughput</div><div id="chart"></div></div>
      </div>
      <div class="grid2">
        <div class="card">
          <div class="card-title">Cloud Intelligence</div>
          <div class="fg"><span class="fl">Network</span><div style="font-family:'JetBrains Mono'; font-size:14px;" id="ai-isp">-</div></div>
          <div class="fg"><span class="fl">Preset</span><div style="font-family:'JetBrains Mono'; font-size:14px; color:var(--accent);" id="ai-preset">-</div></div>
          <button class="btn btn-primary" onclick="fetchIspData()">SYNC DATA</button>
        </div>
        <div class="card">
          <div class="card-title">Adaptive Engine</div>
          <div class="fg"><span class="fl">Status</span><div style="font-family:'JetBrains Mono'; font-size:14px;" id="ai-adaptive-status">ACTIVE</div></div>
          <div class="fg"><span class="fl">Current Gear</span><div style="font-family:'JetBrains Mono'; font-size:14px;" id="ai-gear">-</div></div>
          <div class="fg"><span class="fl">Interventions</span><div style="font-family:'JetBrains Mono'; font-size:14px; color:var(--warning);" id="ai-interv">0</div></div>
        </div>
      </div>
      <div class="card">
        <div class="card-title">Silent Auto-Probe</div>
        <div class="fg"><span class="fl">Next Probe In</span><div style="font-family:'JetBrains Mono'; font-size:14px;" id="ai-timer">15:00</div></div>
        <table class="tbl">
          <thead><tr><th>Target</th><th>Tactic</th><th>Latency</th><th>Status</th></tr></thead>
          <tbody id="tbl-probe"><tr><td colspan="4" style="text-align:center;color:var(--text-muted);" id="probe-empty">AWAITING CYCLE</td></tr></tbody>
        </table>
      </div>
    </div>

    <!-- ENGINE CORE -->
    <div id="view-engine" class="view">
      <div class="grid2">
        <div class="card">
          <div class="card-title">Stealth Configuration</div>
          <div class="fg"><label class="fl">Protocol Algorithm</label>
            <select id="cfg-mode" onchange="dirty()">
              <option value="-1">Mode -1 (Phantom)</option><option value="-2">Mode -2 (Specter)</option>
              <option value="-3">Mode -3 (Wraith)</option><option value="-4">Mode -4 (Shadow)</option>
              <option value="-5">Mode -5 (Silverhand)</option><option value="-6">Mode -6 (Ghost)</option>
              <option value="-7">Mode -7 (Echo)</option><option value="-8">Mode -8 (Mirror)</option>
              <option value="-9">Mode -9 (Void)</option>
            </select>
          </div>
          <div class="fg"><label class="fl">Traffic Scope</label>
            <select id="cfg-filter" onchange="dirty()">
              <option value="0">Global</option><option value="1">Blacklist</option>
              <option value="2">App Target</option><option value="3">Gaming</option>
            </select>
          </div>
          <div class="fg" style="margin-top: 32px;"><label class="fl">System Startup</label>
            <label style="display:flex;align-items:center;gap:10px;font-size:12px;cursor:pointer;">
              <input type="checkbox" id="cfg-autostart" onchange="toggleAutoStart()"> Run at Login
            </label>
          </div>
        </div>
        <div class="card">
          <div class="card-title">DNS Resolution</div>
          <div class="dns-type-selector">
            <button id="btn-dns-preset" class="dns-type-btn active" onclick="setDnsType('preset')">Presets</button>
            <button id="btn-dns-custom" class="dns-type-btn" onclick="setDnsType('custom')">Custom</button>
          </div>
          <div id="dns-preset-wrap" class="fg">
            <label class="fl">Provider</label>
            <select id="dns-preset" onchange="applyPreset()">
              <option value="">-- Select --</option>
              <option value="1.1.1.1|53|std|">Cloudflare (UDP)</option>
              <option value="1.1.1.1|443|doh|https://cloudflare-dns.com/dns-query">Cloudflare (DoH)</option>
              <option value="8.8.8.8|53|std|">Google (UDP)</option>
              <option value="8.8.8.8|443|doh|https://dns.google/dns-query">Google (DoH)</option>
              <option value="9.9.9.9|53|std|">Quad9 (UDP)</option>
              <option value="9.9.9.9|443|doh|https://dns.quad9.net/dns-query">Quad9 (DoH)</option>
              <option value="94.140.14.14|443|doh|https://dns.adguard-dns.com/dns-query">AdGuard (DoH)</option>
            </select>
          </div>
          <div class="fg"><label class="fl">Resolution Stack</label>
            <select id="cfg-dnsmode" onchange="dnsFields();dirty()">
              <option value="std">Standard UDP</option><option value="doh">DNS-over-HTTPS</option>
            </select>
          </div>
          <div style="display:flex;gap:16px;">
            <div class="fg" style="flex:2"><label class="fl">IP Address</label><input type="text" id="cfg-dnsip" oninput="dirty()"></div>
            <div class="fg" id="grp-port" style="flex:1"><label class="fl">Port</label><input type="text" id="cfg-dnsport" oninput="dirty()"></div>
          </div>
          <div class="fg" id="grp-doh"><label class="fl">DoH URL</label><input type="text" id="cfg-dohurl" oninput="dirty()"></div>
        </div>
      </div>
    </div>

    <!-- ACCESS POLICY -->
    <div id="view-filters" class="view">
      <div class="grid2">
        <div class="card">
          <div class="card-title">Blacklist <span id="cnt-blacklist" class="badge-count">0</span></div>
          <div class="fg"><input type="text" id="src-blacklist" placeholder="Search..." oninput="renderList('blacklist')"></div>
          <div class="list-wrap" id="dom-blacklist"></div>
          <div style="display:flex;gap:8px;margin-top:16px;">
            <input type="text" id="inp-bl" placeholder="Domain..." style="flex:1;" onkeypress="if(event.key==='Enter')addItem('blacklist')">
            <button class="btn" onclick="addItem('blacklist')">ADD</button>
            <button class="btn" onclick="clearList('blacklist')">CLEAR</button>
          </div>
        </div>
        <div class="card">
          <div class="card-title">Allowlist <span id="cnt-allowlist" class="badge-count">0</span></div>
          <div class="fg"><input type="text" id="src-allowlist" placeholder="Search..." oninput="renderList('allowlist')"></div>
          <div class="list-wrap" id="dom-allowlist"></div>
          <div style="display:flex;gap:8px;margin-top:16px;">
            <input type="text" id="inp-al" placeholder="Domain..." style="flex:1;" onkeypress="if(event.key==='Enter')addItem('allowlist')">
            <button class="btn" onclick="addItem('allowlist')">ADD</button>
            <button class="btn" onclick="clearList('allowlist')">CLEAR</button>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-title" style="display:flex;justify-content:space-between;align-items:center;">
          <span>Target Applications</span>
          <button class="btn btn-primary" onclick="openAppModal()">MANAGE</button>
        </div>
        <div class="list-wrap" id="dom-allowedapps" style="height: 140px;"></div>
      </div>
    </div>

    <!-- DIAGNOSTICS -->
    <div id="view-tester" class="view">
      <div class="card">
        <div class="card-title" style="display:flex;justify-content:space-between;align-items:center;">
          <span>Matrix Diagnostic (<span id="combo-count">0</span> Combos)</span>
          <div style="display:flex;gap:10px;">
            <input type="text" id="test-host" value="discord.com" style="width: 200px; padding: 6px 10px;">
            <button class="btn btn-primary" id="btn-probe" onclick="runProbe()">RUN PROBE</button>
            <button class="btn" id="btn-stop" onclick="stopProbe()" style="display:none;">STOP</button>
          </div>
        </div>
        <div id="probe-progress" style="display:none; margin-bottom:16px;">
          <div style="display:flex;justify-content:space-between;font-size:10px;font-weight:700;margin-bottom:4px;color:var(--text-muted);">
            <span id="probe-status">RUNNING...</span><span id="probe-pct">0%</span>
          </div>
          <div style="background:var(--bg-main);height:4px;"><div id="probe-bar" style="height:100%;background:var(--accent);width:0%;"></div></div>
        </div>
        <div class="list-wrap" style="height: 350px;">
          <table class="tbl">
            <thead><tr><th>#</th><th>DNS</th><th>Target</th><th>Mode</th><th>Result</th><th>Lat</th></tr></thead>
            <tbody id="tbl-body"><tr><td colspan="6" style="text-align:center;color:var(--text-muted);">READY</td></tr></tbody>
          </table>
        </div>
        <div id="oracle-box" class="oracle-box">
          <div id="oracle-txt" class="oracle-txt">RECOMMENDATION</div>
          <button class="btn btn-primary" onclick="applyOracle()">APPLY</button>
        </div>
      </div>
    </div>

    <!-- LIVE FEED -->
    <div id="view-logs" class="view">
      <div class="card" style="display:flex;flex-direction:column;height:calc(100vh - 100px);">
        <div class="card-title" style="display:flex;justify-content:space-between;margin-bottom:0;">
          <span>Engine Log</span><span id="log-lines" style="color:var(--text-muted);font-size:10px;">0 LINES</span>
        </div>
        <pre id="log-area" style="flex:1;background:var(--bg-main);border:1px solid var(--border);padding:16px;overflow-y:auto;font-family:'JetBrains Mono',monospace;font-size:11px;color:var(--text-muted);white-space:pre-wrap;margin-top:16px;">AWAITING TELEMETRY</pre>
      </div>
    </div>

    <!-- UPDATES -->
    <div id="view-update" class="view">
      <div class="card" style="text-align:center;padding:60px 20px;">
        <h2 style="font-size:24px;font-weight:700;margin-bottom:16px;letter-spacing:2px;">UPDATE CENTER</h2>
        <div id="upd-badge" class="badge" style="display:inline-block;margin-bottom:24px;">CHECKING...</div>
        <div id="upd-msg" style="font-size:12px;color:var(--text-muted);margin-bottom:32px;">CONTACTING SERVER</div>
        <div style="display:flex;justify-content:center;gap:16px;">
          <button class="btn btn-primary" onclick="checkUpdate()">CHECK FOR UPDATES</button>
          <button class="btn" onclick="openLink('https://github.com/gokhazen/HayaletDPI/releases')">RELEASES PAGE</button>
        </div>
      </div>
    </div>

    <!-- ABOUT -->
    <div id="view-about" class="view">
      <div class="card">
        <h2 style="font-size:24px;font-weight:700;margin-bottom:4px;letter-spacing:2px;">HAYALET_DPI</h2>
        <p style="font-size:12px;color:var(--text-muted);font-family:'JetBrains Mono';margin-bottom:24px;">v0.6.1-ZERO // GOKHAN OZEN</p>
        <button class="btn" onclick="openLink('https://github.com/gokhazen/HayaletDPI')" style="margin-right:10px;">GITHUB</button>
        <button class="btn" onclick="openLink('https://gman.dev/hayalet')">WEBSITE</button>
      </div>
    </div>

  </div>
</div>

<div class="action-bar" id="action-bar">
  <p>UNSAVED CHANGES</p>
  <div style="display:flex;gap:10px;">
    <button class="btn" style="border:1px solid #000;" onclick="discardCfg()">DISCARD</button>
    <button class="btn btn-primary" style="background:#000;color:var(--accent);" onclick="saveConfig()">APPLY & RESTART</button>
  </div>
</div>

<script>
{js_code}
</script>
</body>
</html>
"""

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(new_html)

print("Generated new UI successfully.")
