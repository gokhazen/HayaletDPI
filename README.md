<div align="center">

<img src="https://raw.githubusercontent.com/gokhazen/HayaletDPI/main/src/icon.ico" alt="HayaletDPI" width="80">

# HayaletDPI

High-performance Deep Packet Inspection circumvention engine for Windows.

[![Release](https://img.shields.io/github/v/release/gokhazen/HayaletDPI?style=flat-square&color=3b82f6&label=Latest)](https://github.com/gokhazen/HayaletDPI/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2B-blue?style=flat-square&logo=windows)](https://github.com/gokhazen/HayaletDPI/releases)
[![Built with](https://img.shields.io/badge/Built%20with-C%20%2B%20C%2B%2B-orange?style=flat-square)](src/)

[Download](https://github.com/gokhazen/HayaletDPI/releases/latest) | [Website](https://gman.dev/hayalet) | [Developer](https://gman.dev)

</div>

---

## What is HayaletDPI?

HayaletDPI ("Hayalet" translates to Ghost in Turkish) is an open-source Windows application designed to circumvent Deep Packet Inspection (DPI) systems used by ISPs and governments to monitor and block internet traffic. By manipulating TCP and TLS packet structures at the kernel level, HayaletDPI ensures that outgoing traffic remains unrecognisable to censorship middleboxes while staying completely transparent to the destination servers.

The core architecture builds upon GoodbyeDPI and utilizes the WinDivert kernel driver for low-level packet capture and manipulation.

---

## Features

### Bypass Engine
- **Multiple Stealth Modes**: A variety of operational modes ranging from standard to highly aggressive fragmentation.
- Each mode applies distinct packet fragmentation, dummy packet injection, TTL manipulation, and TLS ClientHello desync techniques.
- Per-mode tuning is available via the GUI with seamless engine restarts, removing the need for manual configuration files.

### DNS Configuration
- **DNS Presets**: Integrated autofill for major providers including Cloudflare, Google, Quad9, OpenDNS, AdGuard, and Mullvad.
- **Standard UDP**: Support for port 53 as well as non-standard ports (5053, 853, 5353, 5355).
- **DNS-over-HTTPS (DoH)**: Full endpoint URL support for encrypted DNS resolution.
- Custom IP and port entry for arbitrary network resolvers.

### Access Policy
| Feature | Description |
|---|---|
| Blacklist | Domains that receive DPI bypass treatment. |
| Allowlist | Domains explicitly excluded from DPI manipulation. |
| App Target Mode | Intercept traffic exclusively from selected executable files. |

- Inline management for all filtering lists.
- Process picker to select executables directly from active system processes.
- Configurations persist across sessions.

### Diagnostics and Telemetrics
- Real-time diagnostic probing across multiple DNS nodes and stealth modes to determine the optimal connection profile.
- Result tables displaying latency measurements and bypass success rates.
- Live rolling throughput charts.
- Statistics on packets inspected, bypassed, and the overall circumvention efficiency ratio.
- Raw engine log stream accessible directly from the dashboard.

### Update Center
- Asynchronous querying of the GitHub Releases API.
- Visual indicators for update availability and network connectivity.

---

## Dashboard Architecture

The dashboard is a WebView2-based single-page application embedded inside the native executable. It utilizes a dark-mode design system with a clean, industrial aesthetic.

| Tab | Purpose |
|---|---|
| Telemetrics | Live statistics, throughput charts, and system status. |
| Engine Core | Mode selection, DNS configuration, and filter scope. |
| Access Policy | Blacklist, allowlist, and application targeting. |
| Diagnostics | Matrix probing for optimal configurations. |
| Live Feed | Unfiltered engine log stream. |
| Updates | Version checking. |
| About | Project documentation and developer references. |

**System Tray Integration:**
- Double-click the tray icon to open the dashboard.
- Right-click context menu for engine control, dashboard access, and application exit.
- Singleton guard ensures only one dashboard instance runs at any given time.

---

## Installation

### Pre-built (Recommended)

Download the latest installer from the [Releases](https://github.com/gokhazen/HayaletDPI/releases/latest) page. The installer handles shortcut creation, optional startup registry entries, and bundles all required drivers alongside the WebView2 loader.

### Portable

Extract the architecture-specific binaries from the release archive to any directory. Run the executable as Administrator to initialize the WinDivert driver.

---

## Build from Source

### Prerequisites

| Dependency | Purpose |
|---|---|
| MinGW-w64 (x86_64) | C/C++ compiler toolchain. MSYS2 is recommended. |
| Inno Setup 6 | Installer generation. |
| WebView2 Runtime | Required on the host machine for dashboard rendering. |

### Build Steps

```bat
git clone https://github.com/gokhazen/HayaletDPI.git
cd HayaletDPI/src
build.bat
```

The script will compile the binaries into the `bin/` directory and generate the setup executable in the `releases/` directory.

---

## Antivirus and False Positives

Security software may flag the executable or the installer. This is a common false positive caused by the following factors:

1. **Kernel-level packet driver**: WinDivert intercepts network packets at a low level, a technique commonly monitored by heuristic engines.
2. **Code Signing**: The binaries are not signed with an Extended Validation certificate, which causes automated scanners to treat administrative executables with suspicion.
3. **DPI Manipulation**: Altering TCP/TLS headers dynamically is inherently flagged by network analysis algorithms.

To verify the integrity of the application, you are encouraged to compile the source code manually or cross-reference the release hashes on VirusTotal.

---

## License and Third-Party Credits

HayaletDPI is released under the MIT License. See the LICENSE file for complete terms.

### Third-Party Components

| Component | Author | License | Function |
|---|---|---|---|
| GoodbyeDPI | ValdikSS | Apache 2.0 | Core bypass algorithms |
| WinDivert | basil00 | LGPL v3 | Kernel packet driver |
| uthash | Troy D. Hanson | BSD 1-Clause | Hash table implementations |
| getline | Arryboom | MIT-compatible | POSIX shim |
| WebView2 SDK | Microsoft | EULA | Dashboard renderer |

Complete license texts are located in the `licenses/` directory and the `NOTICE` file.

---

## Disclaimer

HayaletDPI is developed strictly for bypassing network censorship and restoring access to blocked information. Users are responsible for utilizing the software in accordance with the regulations of their respective jurisdictions. The developer assumes no liability for any misuse of this tool.

<div align="center">
Developed by Gokhan Ozen - gman.dev
</div>
