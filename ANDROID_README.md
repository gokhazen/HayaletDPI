# 📱 HayaletDPI Mobile: Android Port Strategy

HayaletDPI'ın yüksek performanslı ve gizlilik odaklı DPI atlatma yeteneklerini Android dünyasına taşıma projesidir. Bu doküman, projenin teknik mimarisini ve geliştirme yol haritasını özetler.

---

## 🚀 Proje Vizyonu
Windows sürümündeki "Stealth Engine" mantığını, Android'in `VpnService` altyapısı ile birleştirerek; root gerektirmeyen, hafif ve modern bir mobil sansür aşma aracı oluşturmak.

## 🛠 Teknik Mimari

### 1. Çekirdek Motor (C & NDK)
Windows sürümündeki C kodları (`fakepackets.c`, `dnsredir.c`), Android **Native Development Kit (NDK)** kullanılarak derlenecektir.
- **Dönüşüm:** WinDivert bağımlılığı kaldırılarak, Linux TUN arayüzü üzerinden ham IP paketlerini okuyup yazacak şekilde optimize edilecek.
- **JNI Bridge:** Java/Kotlin katmanı ile C motoru arasındaki iletişim JNI üzerinden sağlanacak.

### 2. Ağ Katmanı (VpnService)
Android'in standart VPN API'si kullanılacaktır. 
- **No-Root:** Kullanıcıdan özel izinler istemeden, sanal bir ağ arayüzü oluşturulacak.
- **Packet Interception:** Cihazdan çıkan tüm TCP/UDP trafiği yerel bir proxy veya doğrudan TUN üzerinden yakalanarak HayaletDPI motoruna aktarılacak.

### 3. Kullanıcı Arayüzü (UI/UX)
- **Modern Tasarım:** Jetpack Compose veya mevcut WebView altyapısının mobil cihazlara optimize edilmiş versiyonu.
- **Dark Mode & Glassmorphism:** Masaüstü sürümündeki premium estetik korunacak.
- **Tek Dokunuşla Bağlantı:** Basit ve etkili bir "Connect" butonu.

## 🗺 Yol Haritası (Roadmap)

### 📍 Faz 1: İskelet Yapı (Skeleton)
- [ ] Android Studio projesinin kurulumu.
- [ ] Temel `VpnService` sınıfının yazılması.
- [ ] Cihaz trafiğinin başarıyla VPN tüneline yönlendirilmesi.

### 📍 Faz 2: Motor Entegrasyonu (Engine Port)
- [ ] HayaletDPI C dosyalarının (`src/`) NDK ile derlenmesi.
- [ ] Paket manipülasyon mantığının (SNI fragmentation, vb.) TUN arayüzüne uyarlanması.
- [ ] DNS over HTTPS (DoH) desteğinin eklenmesi.

### 📍 Faz 3: Arayüz ve Optimizasyon
- [ ] Gerçek zamanlı trafik monitörü ekranı.
- [ ] Global bağlantı testinin (Benchmarking) mobile uyarlanması.
- [ ] Batarya kullanım optimizasyonları.

---

## 💎 Neden Harika Bir Side Project?
1. **Low-Level Networking:** TCP/IP protokolleri ve paket yapısı üzerinde derin uzmanlık kazandırır.
2. **Cross-Platform Geliştirme:** C kodunu farklı platformlarda (Windows/Linux/Android) koşturma tecrübesi sağlar.
3. **Gerçek Dünya Sorunu:** İnternet özgürlüğü gibi global ve güncel bir probleme çözüm sunar.

---
**Geliştirici:** Gokhan Ozen
**Lisans:** GNU GPL-3.0
