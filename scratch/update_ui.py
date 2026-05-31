import os
import re

html_path = r'c:\Users\mrander\Desktop\projectsnew\hayaletdpi\ui_v2\index.html'
with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace PWR with SVG
svg_pwr = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" style="width:14px;height:14px"><path d="M18.36 6.64a9 9 0 1 1-12.73 0"></path><line x1="12" y1="2" x2="12" y2="12"></line></svg>'
content = content.replace('>PWR</button>', f'>{svg_pwr}</button>')

# Replace view-stats content
new_view_stats = """    <div id="view-stats" class="view active">
      <div class="grid4" style="margin-bottom: 24px;">
        <div><div class="stat-val" id="stat-proc">0</div><div class="stat-label">Ingress Analyzed</div></div>
        <div><div class="stat-val" id="stat-circ" style="color: var(--accent);">0</div><div class="stat-label">Stealth Bypassed</div></div>
        <div><div class="stat-val" id="stat-ratio">0%</div><div class="stat-label">Efficiency Ratio</div></div>
        <div><div class="stat-val" id="chart-rate">0/s</div><div class="stat-label">Live Throughput</div></div>
      </div>
      
      <!-- Telemetry Visuals -->
      <div class="card" style="margin-bottom: 24px;">
        <div class="card-title" style="display:flex;justify-content:space-between;"><span>Throughput Telemetry</span></div>
        <div id="chart" style="height: 120px;"></div>
      </div>

      <div class="grid2">
        <div class="card">
          <div class="card-title">Network & System Status</div>
          <div class="fg" style="display:flex; justify-content:space-between; align-items:center;">
             <span class="fl" style="margin:0;">ISP / Provider</span>
             <div style="font-family:'JetBrains Mono'; font-size:14px; color:var(--accent);" id="ai-isp">-</div>
          </div>
          <div class="fg" style="display:flex; justify-content:space-between; align-items:center;">
             <span class="fl" style="margin:0;">Engine State</span>
             <div style="font-family:'JetBrains Mono'; font-size:14px; color:#fff;" id="dash-engine-state">STOPPED</div>
          </div>
        </div>
        <div class="card">
          <div class="card-title">Active DNS Route</div>
          <div class="fg" style="display:flex; justify-content:space-between; align-items:center;">
             <span class="fl" style="margin:0;">Resolution Stack</span>
             <div style="font-family:'JetBrains Mono'; font-size:14px; color:#fff;" id="dash-dns-mode">-</div>
          </div>
          <div class="fg" style="display:flex; justify-content:space-between; align-items:center;">
             <span class="fl" style="margin:0;">Resolver Target</span>
             <div style="font-family:'JetBrains Mono'; font-size:14px; color:var(--accent);" id="dash-dns-target">-</div>
          </div>
        </div>
      </div>
    </div>"""

# Find and replace <div id="view-stats" class="view active"> ... </div>
# using regex
view_stats_pattern = re.compile(r'    <div id="view-stats" class="view active">.*?    </div>\n\n    <!-- ENGINE CORE -->', re.DOTALL)
content = view_stats_pattern.sub(new_view_stats + '\n\n    <!-- ENGINE CORE -->', content)

# Update syncState to also update the new dashboard elements
js_insert = """
      document.getElementById('dash-engine-state').textContent = s.running ? "ONLINE" : "OFFLINE";
      document.getElementById('dash-engine-state').style.color = s.running ? "var(--accent)" : "var(--danger)";
      document.getElementById('dash-dns-mode').textContent = document.getElementById('cfg-dnsmode').value === 'doh' ? 'DNS-over-HTTPS' : 'Standard UDP';
      document.getElementById('dash-dns-target').textContent = document.getElementById('cfg-dnsip').value + ':' + document.getElementById('cfg-dnsport').value;
"""
content = content.replace("setEngineUI(s.running);", "setEngineUI(s.running);" + js_insert)

with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("UI updated successfully.")
