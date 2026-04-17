<div align="center">

<img src="https://raw.githubusercontent.com/gokhazen/HayaletDPI/main/src/icon.ico" alt="HayaletDPI" width="80">

# HayaletDPI

**v0.6.0 Platinum Edition**

High-performance Deep Packet Inspection circumvention engine for Windows.

[![Release](https://img.shields.io/github/v/release/gokhazen/HayaletDPI?style=flat-square&color=3b82f6&label=Latest)](https://github.com/gokhazen/HayaletDPI/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2B-blue?style=flat-square&logo=windows)](https://github.com/gokhazen/HayaletDPI/releases)
[![Built with](https://img.shields.io/badge/Built%20with-C%20%2B%20C%2B%2B-orange?style=flat-square)](src/)

[📥 Download](https://github.com/gokhazen/HayaletDPI/releases/latest) · [🌐 Website](https://gman.dev/hayalet) · [👤 Developer](https://gman.dev)

</div>

---

## What is HayaletDPI?

HayaletDPI (*"Hayalet"* means Ghost in Turkish) is an open-source Windows application that circumvents **Deep Packet Inspection (DPI)** — the technology ISPs and governments use to detect and block internet traffic. By manipulating TCP/TLS packet structures at the kernel level, HayaletDPI makes your traffic unrecognisable to censorship middleboxes while remaining completely transparent to real servers.

It builds on top of the legendary [GoodbyeDPI](https://github.com/ValdikSS/GoodbyeDPI) and uses the [WinDivert](https://github.com/basil00/WinDivert) kernel driver for low-level packet capture and manipulation.

---

## ✨ Features

### 🔬 Bypass Engine
- **9 Stealth Modes** — Phantom, Specter, Wraith, Shadow, Silverhand *(recommended)*, Ghost, Echo, Mirror, Void
- Each mode applies a different combination of packet fragmentation, fake packets, TTL manipulation, and TLS ClientHello desync techniques
- Per-mode tuning via GUI with live restart — no manual INI editing required

### 🌐 DNS Configuration
- **14+ DNS Presets** — one-click auto-fill for Cloudflare, Google, Quad9, OpenDNS, AdGuard, Mullvad, NextDNS, DNS0.eu, Comodo
- **Standard UDP** (port 53 and non-standard ports: 5053, 853, 5353, 5355)
- **DNS-over-HTTPS (DoH)** — full endpoint URL support
- Custom IP + port entry for any resolver

### 🛡️ Access Policy
| Feature | Description |
|---|---|
| Blacklist | Domains that receive DPI bypass treatment |
| Allowlist | Domains explicitly excluded from bypass |
| App Target Mode | Intercept traffic only from selected executables |

- Inline edit/delete for all lists
- Visual process picker — select any running `.exe` from a live list
- Configuration persisted to `userfiles/` and survives restarts

### 🔭 O.R.A.C.L.E. Diagnostics
- Exhaustive **171-combination probe** — 19 DNS nodes × 9 stealth modes
- Covers all protocol variants: Standard UDP, non-standard ports, DoH
- Real-time results table with latency measurements
- **"Apply Config"** button: one click to write the best-found profile into Engine Core
- Stoppable at any point with a Stop button

### 📊 Telemetrics
- Live 60-second rolling throughput chart
- Packets inspected, bypassed, and circumvention efficiency ratio
- Engine log stream (raw output from `hayalet.log`)

### 🔄 Update Center
- Queries the **GitHub Releases API** asynchronously — UI never freezes
- Visual badge: ✓ Up to Date / ⬆ Update Available / ✗ Connection Failed
- One-click link to the latest release page

---

## 🖥️ Dashboard (V2 Platinum UI)

The V2 dashboard is a **WebView2-based** single-page application embedded inside the executable. It features a sidebar navigation, Inter + JetBrains Mono typography, and a full dark-mode design system.

| Tab | Purpose |
|---|---|
| Telemetrics | Live stats, chart, and log stream |
| Engine Core | Mode, DNS, filter scope configuration |
| Access Policy | Blacklist / allowlist / app targeting |
| Diagnostics | O.R.A.C.L.E. matrix probe |
| Live Feed | Raw engine log stream |
| Updates | GitHub release checker |
| About | Project and developer links |

**Tray integration:**
- Double-click tray icon → open dashboard
- Right-click → Start/Stop Engine, Restart Engine, Open Dashboard, Exit
- Only **one dashboard instance** can be open at a time (singleton guard)

---

## 📂 Project Structure

```
hayaletdpi/
├── src/                     # C/C++ source files
│   ├── hayalet.h            # Shared API & engine config struct
│   ├── version.h            # Single-source version constant
│   ├── gui.c                # Tray host & WinMain entry point
│   ├── webview_v2.cc        # V2 dashboard bridge (WebView2 + JS API)
│   ├── fakepackets.c        # Fake packet injection engine
│   ├── blackwhitelist.c     # Hash-table domain filter
│   ├── dnsredir.c           # DNS connection tracker & redirector
│   ├── hayalet.c            # Core bypass engine orchestrator
│   ├── service.c            # Engine thread management
│   ├── ttltrack.c           # TTL-based connection tracking
│   └── build.bat            # One-command build script
├── ui_v2/
│   └── index.html           # V2 SPA dashboard (self-contained)
├── licenses/                # Third-party license texts
├── userfiles/               # Runtime user configuration
│   ├── settings.ini         # Engine settings (written by dashboard)
│   ├── blacklist.txt        # DPI bypass domain list
│   ├── allowlist.txt        # Bypass exclusion list
│   └── allowedapps.txt      # App-mode executable targets
├── NOTICE                   # Third-party attribution (Apache 2.0 §4)
├── LICENSE                  # MIT License
└── setup.iss                # Inno Setup installer script
```

---

## 🚀 Installation

### Pre-built (Recommended)

Download the latest installer from [**Releases**](https://github.com/gokhazen/HayaletDPI/releases/latest):

```
HayaletDPI_Setup_v0.6.0.exe
```

The installer:
- Creates shortcuts (optional desktop icon)
- Provides an optional "Launch on Windows startup" task
- Bundles all required drivers and the WebView2 loader

### Portable

Extract the contents of `bin/x86_64/` to any folder. Run `hayalet.exe` as Administrator.

---

## 🔧 Build from Source

### Prerequisites

| Tool | Purpose |
|---|---|
| [MinGW-w64](https://www.mingw-w64.org/) (x86_64, MSYS2 recommended) | C/C++ compiler toolchain |
| [Inno Setup 6](https://jrsoftware.org/isinfo.php) | Installer generation |
| Windows 10+ with WebView2 Runtime | Dashboard rendering |

### Build Steps

```bat
git clone https://github.com/gokhazen/HayaletDPI.git
cd HayaletDPI/src
build.bat
```

Output:
- **Portable:** `bin/x86_64/`
- **Installer:** `releases/HayaletDPI_Setup_v0.6.0.exe`

---

## 🛑 Antivirus & False Positives

You may see a Windows Defender warning when running the installer or executable.

**This is a false positive.** Contributing factors:

1. **Kernel-level packet driver** — WinDivert intercepts and modifies network packets, a technique shared by security software *and* malware, triggering heuristic engines
2. **No EV code signing** — open-source projects rarely have expensive Extended Validation certificates; unsigned admin-privilege binaries are frequently flagged by ML-based scanners
3. **DPI manipulation** — manipulating TCP/TLS headers is inherently suspicious to automated analysis systems

To verify integrity, build from source or check the release hashes on [VirusTotal](https://www.virustotal.com).

---

## ⚖️ License & Third-Party Credits

HayaletDPI is released under the **MIT License**. See [LICENSE](LICENSE) for full terms.

### Third-Party Components

| Component | Author | License | Use |
|---|---|---|---|
| [GoodbyeDPI](https://github.com/ValdikSS/GoodbyeDPI) | ValdikSS | Apache 2.0 | Core bypass algorithms |
| [WinDivert](https://github.com/basil00/WinDivert) | basil00 | LGPL v3 | Kernel packet driver |
| [uthash](http://troydhanson.github.com/uthash/) | Troy D. Hanson | BSD 1-Clause | Hash table |
| [getline](https://github.com/Arryboom/fgets_replacement_in_win32) | — | MIT-compatible | POSIX shim |
| [WebView2 SDK](https://aka.ms/webview2) | Microsoft | EULA (aka.ms/webview2eula) | V2 dashboard renderer |

Full license texts are in the [`licenses/`](licenses/) directory and the [`NOTICE`](NOTICE) file.

---

## ⚠️ Disclaimer

HayaletDPI is intended for bypassing **network censorship** and restoring access to legitimately blocked content. Use it responsibly and in accordance with the laws of your jurisdiction. The author assumes no liability for misuse.

---

<div align="center">

Made with ❤️ by [Gokhan Ozen](https://gman.dev) · [gman.dev/hayalet](https://gman.dev/hayalet)

</div>
