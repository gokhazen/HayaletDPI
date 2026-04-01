# 👻 HayaletDPI v0.5.2

**HayaletDPI** is a high-performance, stealthy Deep Packet Inspection (DPI) circumvention utility for Windows. This project is a modern evolution of DPI bypass techniques, featuring a powerful GUI, automated protocol benchmarking, and a clean, portable architecture.

Developed and maintained by **Gökhan Özen**.

---

## 🌟 Key Features

-   **Stealth Engine:** 9 different bypass modes (Phantom, Specter, Wraith, etc.) to evade censorship.
-   **Clean Architecture:** Separated core binaries (`libs/`) from user configuration (`userfiles/`).
-   **Automated Benchmarking:** Integrated "Protocol Tester" to find the best bypass mode for your network.
-   **Application Filtering:** Focus bypass only on specific apps (e.g., Discord/Roblox) or use global mode.
-   **Built-in DNS Commander:** Easy toggle between standard UDP DNS and Secured DoH (DNS-over-HTTPS).
-   **Dashboard & Analytics:** Real-time throughput graph and bypass ratio monitoring.

---

## 📂 Project Structure

To maintain a professional environment, the project is organized as follows:

-   **`hayalet.exe`**: The main executable.
-   **`libs/`**: System-critical "hardware" files (`WinDivert.dll`, `WinDivert64.sys`).
-   **`userfiles/`**: Your personal configuration and lists.
    -   `settings.ini`: Persisted application settings.
    -   `blacklist.txt`: Domains to be bypassed.
    -   `allowlist.txt`: Domains to be ignored.
    -   `allowedapps.txt`: Applications allowed in App Filter mode.
    -   `hayalet.log`: Detailed engine logs.

---

## 🚀 Installation & Build

### Download (Releases)
Download the latest `HayaletDPI_Setup_vX.X.X.exe` from the [Releases](https://github.com/hayaletdpi/hayaletdpi/releases) section. The installer will automatically set up shortcuts and the directory structure.

### Manual Build
If you prefer to build from source:
1.  Clone the repository.
2.  Ensure you have **MinGW-w64** and **Inno Setup** installed and in your `PATH`.
3.  Run `src/build.bat`.
4.  The output will be generated in `bin/x86_64/` and `releases/`.

---

## ⚖️ License & Credits

Distributed under the **GNU GPL-3.0 License**. See [LICENSE](LICENSE) for more information.

**Credits:**
-   **Lead Developer:** Gökhan Özen
-   **Original Core Concept:** Based on the legendary *GoodbyeDPI* by ValdikSS.
-   **Dependencies:** Uses the high-performance *WinDivert* network driver.

---

## 🛡️ Disclaimer
This tool is intended for educational purposes and to bypass overly restrictive network censorship. Please use it responsibly and in accordance with your local laws.
