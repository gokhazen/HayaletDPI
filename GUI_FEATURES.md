# HayaletDPI GUI Özellikleri ve Teknik Uygulama Rehberi

Bu belge, HayaletDPI'ın mevcut Win32 arayüzündeki her bir özelliğin **teknik işleyiş mantığını** ve **arka plan (backend) bağlantılarını** detaylandırır. Bu rehber, yeni WebView-tabanlı (V2) arayüzün geliştirilmesinde temel algoritma sözlüğü olarak kullanılacaktır.

---

## 1. Sistem Yönetimi ve Motor Kontrolü (Lifecycle)
Uygulamanın kalbi olan motorun yönetimi şu teknik adımlarla gerçekleşir:

### A. Uygulama Başlangıcı (WinMain)
- **Mutex Guard**: `HayaletDPI_GokhanOzen_UniqueMutex` adında bir sistem mutex'i oluşturulur. Eğer varsa uygulama sessizce kapanır.
- **UserFiles Konumlandırma**: `_mkdir("userfiles")` ile ayarların tutulacağı klasör garantiye alınır.
- **Engine Silent Start**: Kullanıcı müdahalesi beklemeksizin `StartEngine(TRUE)` çağrılır.

### B. Motor Başlatma (StartEngine)
- **Thread Yönetimi**: UI'ın donmaması için motor `CreateThread` ile `dpi_thread` fonksiyonu üzerinde çalıştırılır.
- **API Bağlantısı**: `Hayalet_RunConfig(config)` fonksiyonuna INI dosyasından okunan `HayaletConfig` yapısı (struct) gönderilir.
- **Durum Takibi**: Global `engine_running` bayrağı `1` yapılır ve sistem tepsisi ikonu güncellenir.

### C. Motor Durdurma ve Yeniden Başlatma (Stop / Restart)
- **Stop Logic**: `stop_hayalet()` çağrılır -> Bu fonksiyon backend'deki `exiting=1` bayrağını set eder -> Motor döngüsünden çıkar -> `deinit_all()` ile WinDivert sürücüsü serbest bırakılır.
- **Restart Logic**: `StopEngine(FALSE)` -> `Sleep(500)` -> `StartEngine(FALSE)` sırasıyla çalıştırılır.

---

## 2. Dashboard: Veri İzleme ve Grafik Sistemi

### A. Paket Sayaçları (Real-Time Stats)
- **Mantık**: Her 1000ms'de bir (`WM_TIMER`) `UpdateStatusText` fonksiyonu çağrılır.
- **Veri Kaynağı**: `hayalet.c` içerisindeki `packets_processed` ve `packets_circumvented` global `uint64_t` değişkenlerinden anlık değerler okunur.
- **Hesaplama**: `(Bypass / Toplam) * 100` formülüyle başarı oranı hesaplanıp UI'a basılır.

### B. Trafik Akış Grafiği (Traffic Graph)
- **Örnekleme**: Son 60 saniyenin verisi `traffic_history[60]` ve `bypass_history[60]` dizilerinde tutulur.
- **Kaydırma**: Her saniye `memmove` ile eski veriler sola kaydırılır, en sağa (index 59) o saniyedeki paket farkı (`current - previous`) eklenir.
- **Çizim (GDI)**: `DrawTrafficGraph` fonksiyonu dizideki en yüksek değeri (`max_v`) bulur ve ekranın yüksekliğini buna göre oranlayarak (`dy = height / max_v`) çizgileri çizer.

---

## 3. Engine: Yapılandırma ve DNS Ayarları

### A. Stealth Engine Modes
- **Mantık**: ComboBox'tan seçilen mod (-1 ile -9 arası), INI dosyasına `Mode` anahtarı altında kaydedilir.
- **Uygulama**: `ApplyEngineMode(int mode)` fonksiyonu, seçilen moda göre backend'deki `do_fragment_http`, `do_fake_packet`, `do_auto_ttl` gibi bayrakları otomatik set eder.

### B. DNS ve DoH Yönetimi
- **Preset Mantığı**: Sağlayıcı listesinden biri seçildiğinde `dns_table` (struct dizisi) içindeki IP ve URL değerleri ilgili input alanlarına otomatik kopyalanır.
- **DoH Geçişi**: `IDC_RAD_DNS_SECURE` seçiliyse `WritePrivateProfileString` ile `DoH=1` set edilir. Motor başladığında DNS sorgularını UDP yerine belirtilen HTTPS URL'sine yönlendirir.

---

## 4. Filtreleme: Uygulama ve Alan Adı Yönetimi

### A. Targeted Apps (Uygulama Bazlı Filtre)
- **Picker**: `CreateToolhelp32Snapshot` ile çalışan süreçler taranır, seçilenler `allowedapps.txt` dosyasına alt alta yazılır.
- **Backend Kontrolü**: Motor her yeni bağlantıda (Flow Established) `IsProcessAllowed(pid)` fonksiyonunu çağırır. Eğer paket izin verilen bir uygulamadan gelmiyorsa, bypass uygulanmadan `WinDivertSend` ile direkt geçilir.

### B. Alan Adı Listeleri (Allowlist / Blacklist)
- **Inline Editor**: Listbox'taki "Add" butonu, yazılan metni hem listeye ekler hem de anında `allowlist.txt` veya `blacklist.txt` dosyasına kaydeder.
- **Suffix Match**: `IsAllowlisted` fonksiyonu, gelen SNI hostname'inin sonunu (suffix) kontrol eder. (Örn: `discord.com` ekliyse, `cdn.discord.com` da otomatik muaf tutulur).

---

## 5. Teşhis ve Analiz Araçları

### A. Protocol Benchmark (Protocol Tester)
- **Algoritma**:
    1. Hedef host (discord.com) IP'ye çözülür (`gethostbyname`).
    2. `-1`den `-9`a kadar tüm modlar için bir döngü başlatılır.
    3. Her mod için sırayla:
        - `connect()` ile Port 80 ve 443 test edilir.
        - Ham paketlerle UDP 53 (DNS) sorgusu gönderilir.
        - Ham paketle QUIC Handshake denemesi yapılır.
    4. Gecikme süreleri (`GetTickCount()` farkı) tabloya yazılır.
- **Puanlama**: Başarılı bağlantılara sabit puanlar (HTTP: 25, HTTPS: 35 vb.) ve hıza göre bonus puanlar eklenerek `X/100` skoru oluşturulur.

### B. Canlı Log İzleyici (Log Viewer)
- **Mekanizma**: 500ms'lik bir timer ile `hayalet.log` dosyası `fopen("rb")` ile açılır.
- **Stream**: `fseek` ile dosyanın en son kaldığı pozisyona (`last_pos`) gidilir, yeni eklenen satırlar okunur ve `EM_REPLACESEL` ile metin kutusuna eklenir.
- **Auto-Scroll**: `GetScrollInfo` ile kullanıcı eğer en aşağıdaysa metin eklendikten sonra `EM_SCROLLCARET` ile kaydırma yapılır.

---

## 6. WebView V2 Geliştirme Notları (Planlanan)
Yeni arayüzde bu Win32 mantıkları şu şekilde modernize edilecektir:
- **Veri Taşıma**: C sayaçları (packets_processed vb.), `webview_bind` kullanılarak JS tarafına JSON objesi olarak gönderilecektir.
- **Grafik**: GDI çizim kodları yerine **Chart.js** veya basit **SVG** animasyonları kullanılacaktır.
- **Ayarlar**: INI dosyası yönetimi JS tarafından gelen bir `SaveConfig(json)` çağrısı ile C tarafında işlenecektir.
- **Log**: Log dosyası `tail` mantığıyla okunup JS tabanlı şık bir konsol ekranına basılacaktır.
tik olarak en aşağı kayar.

---
> [!IMPORTANT]
> Tüm değişikliklerin geçerli olması için ana paneldeki "Apply Settings" butonuna basılması gerekir. Bu buton hem ayarları `settings.ini`ye kaydeder hem de motoru yeni ayarlarlar ile otomatik olarak yeniden başlatır.
