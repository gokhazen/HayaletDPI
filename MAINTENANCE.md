# 🛠️ HayaletDPI Update & Release Checklist

Bu dosya, HayaletDPI projesine yeni bir özellik eklendiğinde veya hata giderildiğinde izlenmesi gereken standart prosedürleri içerir.

## 🕒 Her Güncelleme Öncesi (Kayıt Öncesi)
- [ ] **Kod Temizliği:** Tüm `TODO` ve `printf` debug satırlarının temizlendiğinden emin olun.
- [ ] **Kütüphane Kontrolü:** Yeni bir kütüphane eklendiyse `Makefile` içindeki `LIBS` kısmına eklendiğinden emin olun.
- [ ] **Dil Birliği:** Tüm kullanıcı arayüzü ve mesaj kutularının (MessageBox) İngilizce (Global) olduğundan emin olun.

## 🔢 Versiyon Güncelleme (Sync)
Versiyon numarası her zaman şu iki noktada eşzamanlı güncellenmelidir:
1.  **Ana Dizin:** `VERSION` dosyası (Örn: `0.5.2`)
2.  **Kod Dizini:** `src/version.h` (`HAYALET_VERSION` ve `HAYALET_FULL_TITLE`)

## 🏗️ Derleme & Test (Build Verification)
- [ ] **Clean Build:** `src/build.bat` dosyasını çalıştırın.
- [ ] **Hata Denetimi:** Terminalde `[ERROR]` veya `Compilation failed` yazısı olmadığına emin olun.
- [ ] **Arayüz Testi:** `bin/x86_64/hayalet.exe` dosyasını çalıştırın ve:
    - Sol üstteki başlıkta yeni sürüm numarasını kontrol edin.
    - "Update" ve "About" sekmelerindeki sürüm numaralarının doğru göründüğünden emin olun.
    - Tüm sekmelerin (Dashboard, Engine, Filter, Update, About) görünür ve tıklanabilir olduğunu doğrulayın.

## 📦 Dağıtım Hazırlığı
- [ ] **Portable Klasör:** `bin/x86_64` klasörünün içinde `hayalet.exe`, `WinDivert.dll` ve `WinDivert64.sys` dosyalarının yan yana olduğundan emin olun.
- [ ] **Setup Dosyası:** `releases/` klasöründe yeni sürüm numaralı `.exe` kurulum dosyasının oluştuğunu kontrol edin.

## 🚀 GitHub Yayınlama (Release)
- [ ] **README Güncelleme:** Yeni sürümle gelen kritik bir özellik varsa `README.md` dosyasındaki "Features" kısmına ekleyin.
- [ ] **Tag (Etiket):** GitHub'da yeni bir release oluştururken Tag kısmına `vX.Y.Z` (Örn: `v0.5.2`) yazın.
- [ ] **Release Notes:** Yapılan değişiklikleri içeren madde madde bir liste hazırlayın ve ekleyin.

---
**Gokhan Ozen - HayaletDPI Engineering Standards**
