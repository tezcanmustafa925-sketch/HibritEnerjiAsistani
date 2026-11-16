# Grafik Arayüz Geliştirme Notları

Son güncelleme: **16 Kasım 2026** (Adım 6 yerelde bitti; sıradaki Adım 7: GitHub'a gönderme)

## Hedef

Projeye grafik arayüz eklemek, son hâlini GitHub'a göndermek ve **başka bir
bilgisayarda kurulum yapmadan çalıştırmak**. O bilgisayarda geliştirme yapılmayacak:
GitHub **Releases** sayfasından zip indirilecek, `hea_gui.exe` çift tıklanarak açılacak.

## Verilen kararlar

| Konu | Karar | Neden |
|---|---|---|
| Arayüz kütüphanesi | Dear ImGui 1.91.8 + ImPlot 0.16 | Grafik ve ayar paneli için hafif, README'de zaten planlanmıştı |
| Pencere / çizim | GLFW 3.4 + OpenGL 3.0 | Taşınabilir, kurulumu kolay |
| Bağımlılıklar | CMake `FetchContent`, **git** ile (`GIT_TAG`, `GIT_SHALLOW`) | Elle kopyalama yok. Arşiv (.tar.gz) indirme bu ortamda başarısız oldu, git çalıştı |
| Dağıtım | Tek exe, her şey statik bağlı | Diğer bilgisayarda DLL / Visual C++ Redistributable gerekmesin |
| MSVC çalışma zamanı | `/MT` (kök `CMakeLists.txt`, `CMAKE_MSVC_RUNTIME_LIBRARY`) | "VCRUNTIME140.dll bulunamadı" hatası çıkmasın |
| Font | Roboto Medium, derleme sırasında exe'ye gömülür | Türkçe harfler (ğ, ı, ş, İ) her bilgisayarda görünsün |
| GUI seçeneği | `HEA_BUILD_GUI` varsayılan **OFF** | CI ve CLI etkilenmez; GUI yalnızca istenince derlenir |
| Paket | CMake `install()` + CPack ZIP | Yerelde ve CI'da aynı komutla aynı zip; ek araç gerekmez |
| Release | CI'da `gh release create` (GitHub'ın kendi aracı) | Üçüncü taraf eylem yok; etiket = CMake sürümü denetlenir |

`hea_gui.exe` yalnızca her Windows'ta bulunan DLL'leri kullanıyor
(KERNEL32, USER32, GDI32, SHELL32, IMM32, COMDLG32; OPENGL32 çalışma anında GLFW
tarafından yüklenir), `hea_cli.exe` yalnızca KERNEL32. Bunu artık `paket_dll_bagimliliklari`
testi her derlemede denetliyor. Boyutlar: `hea_gui.exe` ~1.5 MB, `hea_cli.exe` ~0.45 MB,
zip ~950 KB (16 Kasım 2026).

## Yapılanlar

### Adım 1: İskelet ✅
- `apps/gui/` klasörü, pencere + ImGui + ImPlot kurulumu, gömülü Türkçe font.

### Adım 2: Ayarlar paneli + Çalıştır + sonuçlar ✅
- **Sol panel:** çevrim seçimi (Test / Eğitim / Standart grupları + CSV'den ekleme),
  strateji seçimi ve açıklaması, başlangıç SOC'si, sabit ortam sıcaklığı, sabit yük,
  klima, motor soğuk başlangıç, ayar dosyası yükleme / varsayılana dönme.
- **Çalıştır (F5):** panelin altına sabit. Ayar değişince eski sonuç sarı uyarıyla işaretlenir.
- **Üstte ana sonuçlar:** tüketim, toplam yakıt, SOC baş/son, elektrikle geçen süre,
  motor çalıştırma sayısı. Dar pencerede kutucuklar alt satıra geçer.
- **Grafikler:** hız (hedef + gerçekleşen), SOC (pencere sınırlarıyla), toplam yakıt.
  Zaman eksenleri bağlı.
- Pencere ekranın kullanılabilir alanına sığdırılır ve ortalanır (1366x768'de taşmaz),
  yüksek DPI ölçeğine uyar.

### Adım 3: Canlı simülasyon ✅ (13 Kasım 2026)
- **Oynatma çubuğu** (grafiklerin üstünde): Canlı oynat / Baştan oynat, Duraklat / Devam et
  (Boşluk tuşu da), Sona git, hız kaydırıcısı 1–100 kat (logaritmik, varsayılan 10 kat =
  `hea_cli --hiz` ile aynı anlam), ilerleme çubuğu ("510 / 1241 s").
- **Anlık kutucuklar** (canlı oynatma sürerken): hız, motor devri (ya da "kapalı"),
  batarya SOC'si, mod (renkli), toplam yakıt. Bitince özet kutucuklarına döner.
- **Grafikler soldan sağa dolar:** hedef hız çevrimin tamamı için baştan çizili, "şimdi"
  dikey çizgisi, yakıt ekseni oynarken kendini ayarlar. Sol paneldeki tablo
  "Sonuç (şu ana kadar)" başlığıyla ara sonucu gösterir.
- **Mod şeridi:** hız grafiğinin altında, her an hangi modda gidildiği (Elektrikle yeşil,
  Motorla turuncu, Rejeneratif fren mavi, Duruş gri). Anında çalıştırmada da görünür.
- Çalıştır (F5) da aynı yoldan geçer (`LiveRun`'ı hemen sonuna kadar ilerletir);
  canlı oynatma sırasında F5 oynatmayı bırakıp sonucu hemen hesaplar.
- Oynarken ayar değişirse oynatma başladığı ayarlarla sürer, sonuç "eski" işaretlenir.
- Grafikler hizalı (`BeginAlignedPlots`): önceden yakıt grafiği birkaç piksel kaymıştı.
- Dar pencere / büyük ölçek: kutucuklar önce tek satıra (gerekirse orantılı sütunlarla)
  sığdırılır, ilerleme çubuğu yer yoksa alt satıra geçer, grafiklerin en küçük
  yüksekliği yazı boyutuna bağlı. 1256x675, 1000x640 ve 1.5 ölçekte görüntüyle denendi.
- **Yüksek DPI:** varsayılan pencere boyutu ekranın ölçeğiyle büyütülür (ör. %150'de
  2100x1290, sonra ekranın %92 / %88'ine sığdırılır). **Bu bilgisayar %100 olduğu için
  gerçek bir yüksek DPI ekranda denenmedi.**
- Ayrıntı: canlı oynatmanın başında zaman ekseni, veriye oturtmak yerine doğrudan
  çevrimin başı-sonu yapılır; yakıt grafiğinde henüz veri yokken x'i oturtmak bağlı
  ekseni t=0 çevresine (±0.5 s) daraltıyordu.
- **Görünür pencerede denendi** (kullanıcı onayıyla): gerçek fare tıklamasıyla
  "Canlı oynat", 3.36 s arayla iki görüntü: 6 s → 39 s, yani ~9.8 kat (ayar 10 kat).
  Kapanış çıkış kodu 0. Bu denemede bulunan: fare mod şeridinin "mod" ekseni üstündeyken
  ImPlot ekseni mavi vurguluyordu (kilitli eksende anlamsız) → `ImPlotAxisFlags_NoHighlight`.
  `--eylem fare:462:387` ile görünmez modda yeniden üretildi, düzeltme piksel rengiyle
  doğrulandı.

### Adım 4: Strateji karşılaştırması sekmesi ✅ (13 Kasım 2026)
- Sağ taraf artık iki sekme: **Sürüş** (önceki görünüm) ve **Strateji karşılaştırması**.
  Sol paneldeki ayarlar ikisinde de geçerli; alttaki düğme açık sekmenin işini yapar
  ("Çalıştır (F5)" / "Karşılaştır (F5)"), F5 de öyle.
- Hesap `Session::compare`: her strateji için 1 ham koşu + 9 başlangıç SOC'siyle
  doğrusal SOC düzeltmesi, **hea_cli --karsilastir ile aynı core çağrıları, aynı sıra**.
  Süre ölçüldü: bir çevrimde 4 strateji (40 koşu) ~50-80 ms → arka plan iş parçacığı
  gerekmedi; sekme ilk açıldığında kendiliğinden hesaplanır, sonra düğmeyle.
- Ekranda: en az yakan strateji (SOC düzeltmeli), ham ve SOC düzeltmeli çubuk grafik
  (değerler çubukların üstünde, kural tabanlı referans çizgisi), tablo (ham, SOC değişimi,
  SOC düzeltmeli, fark, eşdeğer yakıt, fark), dış değerleme işareti (*) ve dipnot.
  R² ve b (g/kWh) SOC düzeltmeli hücresinin ipucunda.
- Ayar değişince karşılaştırma "eski" işaretlenir; strateji seçimi işaretlemez
  (4 strateji birden karşılaştırılıyor).
- Dar pencerede tablo kısa adlara geçer, grafik açıklaması her zaman çizim alanının
  üstündedir. 1400x860, 1256x675, 1000x640 ve 1.5 ölçekte görüntüyle denendi.

### Adım 5: Dışa aktarma ✅ (13 Kasım 2026)
- İki sekmenin başlık satırının sağında **"CSV olarak kaydet"** ve **"Görüntü kaydet"**
  (dar pencerede alt satıra geçer, dışarı taşmaz).
  - Sürüş sekmesi: adım sonuçları (canlı oynatma sürüyorsa o ana kadarki adımlar),
    `hea_cli --csv` ile **bayt bayt aynı**.
  - Karşılaştırma sekmesi: 4 stratejinin satırları, `hea_cli --karsilastir-csv` ile
    **bayt bayt aynı**.
  - Görüntü: pencerenin o anki karesi PNG. Görüntü alınırken fare pencere dışındaymış
    gibi bir kare daha çizilir: tıklanan düğmenin vurgusu ve ipucu görüntüye girmez.
- Windows "Farklı kaydet" penceresi: önerilen ad (`<çevrim>_<strateji>_adimlar.csv`,
  `<çevrim>_karsilastirma.csv`, `<çevrim>_<strateji>.png`), uzantı yazılmazsa eklenir,
  var olan dosyada üzerine yazma sorulur. Kaydedilince altta "Kaydedildi: ... (1241 adım)".
- **Ortak kod:** adım ve karşılaştırma CSV satırları `apps/common/result_csv` (kütüphane
  `hea_result_csv`) içine taşındı; CLI ve arayüz ikisi de onu kullanır. Dosya açmaz,
  akışa yazar: arayüz Türkçe karakterli yollar için `std::filesystem` ile açar.
  Taşımadan önce/sonra CLI'ın 5 CSV çıktısı (adımlar, 8 MB çoklu koşu, özet,
  karşılaştırma, eğitim verisi) bayt bayt karşılaştırıldı: aynı.
- `--kayit-klasoru <klasör>`: kaydet düğmeleri dosya penceresi açmadan o klasöre yazar
  (testler bunu kullanır).
- **Görünür pencerede denendi** (kullanıcı onayıyla): kaydetme penceresinin başlığı,
  önerilen adı, filtresi doğru; uzantısız adla ".png" / ".csv" eklendi; var olan dosyada
  "Farklı Kaydetmeyi Onayla" sorusu çıktı; görünür pencereden alınan PNG temiz.
  Deneme sırasında pencerelere kullanıcı da tıkladı (bir "Kaydet" ve bir "Evet"): bu yolla
  `OneDrive\Belgeler\sehir_101_model-tabanli_adimlar.csv` oluştu, içeriği CLI ile aynı.
- **Doğrulanmayanlar:** karşılaştırma CSV'si gerçek "Farklı kaydet" penceresiyle
  kaydedilmedi (aynı kod yolu, `--kayit-klasoru` ile test ediliyor); uzantısız ad var olan
  bir dosyayla çakışınca Windows'un sorup sormadığı belirsiz kaldı (o anda pencereye
  kullanıcı da tıkladı).
- **Bilinen sınır:** PNG yazıcı sıkıştırma yapmıyor; 1400x860 görüntü ~3.6 MB.

### Adım 6: Dağıtım paketi ve CI ✅ (16 Kasım 2026, yerelde)
- **Sürüm 0.3.0** (kök `CMakeLists.txt`; arayüzde ve `hea_cli` başlığında görünür).
- **Zip paketi** (`cpack --config <derleme>/CPackConfig.cmake -C Release` →
  `<derleme>/paket/HibritEnerjiAsistani-0.3.0-windows-x64.zip`). Açılınca tek klasör:
  `hea_gui.exe`, `hea_cli.exe`, `data/` (örnek ayar + çevrim), `BENIOKU.txt`
  (UTF-8 BOM'lu, CRLF; sürüm `configure_file` ile yazılır), `lisanslar/` (ImGui MIT,
  ImPlot MIT, GLFW zlib, Roboto Apache 2.0; Apache metni `packaging/lisanslar/` içinde,
  Python `tzdata` paketindeki tam metinden kopyalandı).
- **OpenGL hata kutusu:** önceden OpenGL 3 yoksa (eski sürücü, Uzak Masaüstü, sanal makine)
  program çift tıklanınca hiçbir şey göstermeden kapanıyordu (konsolsuz exe, stderr
  görünmez). Artık normal modda nedenini ve GLFW ayrıntısını bir hata kutusuyla söylüyor
  (`failGraphics`, `gui::showErrorBox`); ekran görüntüsü modunda kutu açılmaz, yalnızca
  stderr. Test için `HEA_TEST_OPENGL_SURUMU=9.9` ortam değişkeni olmayan bir OpenGL sürümü
  isteterek hatayı üretir (`gui_opengl_yok` testi).
- **DLL denetimi** (`paket_dll_bagimliliklari`, `tests/check_dependencies.cmake`):
  `dumpbin /DEPENDENTS` ile exe'lerin yalnızca izinli sistem DLL'lerine bağlı olduğunu
  denetler. Ekran kartı gerekmez, CI'da da çalışır. `dumpbin.exe` ve `link.exe`'ye
  uygulanarak VCRUNTIME140 / MSVCP140 / api-ms-win-crt yakaladığı denendi.
- **CI** (`.github/workflows/ci.yml`):
  - `derle-test` (Windows + Linux, GUI'siz) aynen; eylemler Node 24 sürümlerine geçti
    (`checkout@v5`, `setup-python@v6`), eski "Node.js 20 deprecated" uyarısı gitmeli.
  - `arayuz-paket` (Windows): `HEA_BUILD_GUI=ON` derleme, `ctest -LE gui_ekran`, `cpack`,
    zip `hea-windows-x64` adlı artifact olarak yüklenir (`upload-artifact@v6`). Her push'ta.
  - `yayinla` (yalnızca `v*` etiketinde, iki iş de geçerse): artifact indirilir
    (`download-artifact@v7`), etiket ile zip adındaki sürüm aynı mı denetlenir,
    `gh release create --verify-tag --notes-file ... --generate-notes` ile Release açılır
    (Türkçe kısa kullanım notu + otomatik değişiklik listesi).
- **README:** durum satırı, Releases bağlantısı, "Grafik arayüz" bölümü (paket, ekran,
  tuşlar, kaynaktan derleme, `hea_gui` komut satırı, çıkış kodları), mimari, yol haritası.
  PDF rehberi yeniden üretildi (`python docs/pdf_olustur.py`), sayfaları görüntüyle denetlendi.
- **Doğrulama:**
  - GUI'li derleme 84/84, GUI'siz 40/40, uyarısız.
  - CI'ın `arayuz-paket` adımları yerelde **sıfırdan** (`out/ci-deneme`, bağımlılıklar git'ten
    yeniden indi) aynı komutlarla çalıştırıldı: 41/41 test, zip aynı içerik.
  - Zip, Türkçe karakterli ve boşluklu bir klasöre açıldı; iki exe oradan çalıştı, aynı sonuç
    (0.525 L), exe'ler derleme çıktısıyla bayt bayt aynı, arayüzde "sürüm 0.3.0".
  - `ci.yml` PyYAML ile ayrıştırıldı; etiket denetimi (v0.3.0 geçer, v0.3.1 durur) ve sürüm
    notu üretimi yerelde çalıştırıldı.
- **Doğrulanmayanlar:** iş akışı GitHub'da hiç çalışmadı (push gerekiyor → Adım 7). Hata
  kutusunun kendisi ekranda görülmedi (görünür deneme kullanıcı onayı ister).

### Çekirdekte yapılan tek değişiklik
`loadCycleCsv` ve `loadConfigFile` için `std::istream` alan sürümler eklendi
(`core/include/hea/cycle_io.h`, `config_file.h`). Arayüz, Türkçe karakterli Windows
yollarını `std::filesystem` ile açıp bunlara veriyor. Eski fonksiyonlar aynen çalışıyor,
CLI davranışı değişmedi. Testi: `tests/test_config.cpp` → `testStreamOverloads`.

## Dosya haritası (`apps/gui/`)

| Dosya | Görevi |
|---|---|
| `CMakeLists.txt` | Bağımlılıkları indirir, fontu gömer, `hea_gui_session` kütüphanesi + `hea_gui` exe |
| `main.cpp` | Pencere, OpenGL, ImGui kurulumu; komut satırı; **ekran görüntüsü modu ve eylem sürücüsü**; "Görüntü kaydet" isteğini karşılar (kareyi okuyup PNG'ye yazar) |
| `app.h/.cpp` | Arayüzün kendisi (paneller, sekmeler, grafikler, kaydet düğmeleri). Hesap yapmaz |
| `session.h/.cpp` | ImGui'siz mantık: çevrim listesi, ayarlar, simülasyonu çalıştırma, strateji karşılaştırması (`compare`), dosya okuma, CSV kaydetme |
| `live_run.h/.cpp` | ImGui'siz: simülasyonu adım adım / gerçek zamana bağlı ilerletme (canlı oynatma ve Çalıştır) |
| `../common/result_csv.h/.cpp` | CLI ile ortak: adım ve karşılaştırma CSV satırları (`hea_result_csv`) |
| `png_writer.h/.cpp` | Dış kütüphanesiz PNG yazıcı (sıkıştırmasız; ekran görüntüleri) |
| `platform.h/.cpp` | Windows dosya açma / "Farklı kaydet" pencereleri, UTF-8 komut satırı argümanları, hata kutusu |
| `embedded_font.h` | Gömülü fontun bildirimi (tanımı derlemede üretilir) |
| `../../packaging/` | Zip için `BENIOKU.txt.in` ve `lisanslar/Roboto_LICENSE.txt` |
| `../../tests/check_dependencies.cmake` | Exe'lerin DLL bağımlılığı denetimi |

## Test altyapısı

Toplam **84 test** (GUI'siz derlemede 40; CI'ın GUI'li işinde `-LE gui_ekran` ile 41),
hepsi geçiyor, derleme uyarısız (16 Kasım 2026, Adım 6 sonu).

**Araçlar**
- **Ekran görüntüsü modu:** `hea_gui --ekran-goruntusu x.png` programı **görünmez
  pencerede** çizer, PNG'ye yazar, durum satırını stdout'a basar, çıkar.
  Kullanıcının ekranında pencere açılmaz. **Bu modda her kare 1/60 s sayılır** (gerçek
  saat değil): canlı oynatma testleri deterministik.
- **Eylem sürücüsü:** `--eylem tikla:<kontrol>`, `--eylem kaydir:<kontrol>:<0..1>`,
  `--eylem tus:F5`, `--eylem tus:Space`, `--eylem bekle:<kare>`,
  `--eylem fare:<x>:<y>` (tıklamadan fareyi o piksele götürür). Gerçek fare/klavye
  olayları taklit edilir. Kontrol adları `app.h` içinde `findWidget` açıklamasında.
- **`--kayit-klasoru <klasör>`:** kaydet düğmeleri dosya penceresi açmadan yazar.
- **Durum satırı** (`App::statusLine`), testlerin denetlediği alanlar: `cevrim`, `strateji`,
  `soc0`, `ortam`, `yuk`, `klima`, `soguk`, `ayar`, `kirli`, `sonuc=<çevrim>/<strateji>`,
  `yakit_L`, `soc_son`, `canli=yok|oynuyor|duraklatildi|bitti`, `adim=<k>/<N>`, `hiz`,
  `sekme=surus|karsilastirma`, `kars=<çevrim>|yok`, `kars_kirli`,
  `kars_<strateji>=<düzeltilmiş>/<eşdeğer>`, `kayit=<son kaydedilen dosya>|-`, `hata`.

**Testler**
- **`test_gui_session`** (birim, ImGui'siz):
  - Session: çevrim listesi, 4 strateji, ayar geçersiz kılma, hatalar, Türkçe
    karakterli yollarla dosya okuma, ayar dosyası; sonuç core / CLI ile birebir aynı.
  - `LiveRun`: 4 stratejide parça parça ilerletme = `Session::run` (her adım birebir),
    gerçek zaman birikimi (60 x 1/60 s x 10 kat = tam 10 adım), uzun kare sınırı
    (`kMaxFrameSeconds` = 0.25 s), geçersiz süre/hız, hatalı başlatmada eski çalıştırmanın
    korunması, çalışırken Session değişse de etkilenmeme, taşıma (move).
  - `compare`: core'u doğrudan çağırmakla birebir aynı; ayarların etkisi; hatalar.
  - CSV kaydetme: Türkçe adlı yollar, başlık / satır / sütun sayıları, yarım kalmış
    canlı çalıştırma, yazılamayan klasör. PNG yazıcı.
- **Senaryo testleri (`gui_senaryo_*`, betik `tests/check_gui_png.cmake`, `EXPECT_REGEX`
  ile düzenli ifade denetimi de yapar):** açılış, strateji seçimi, "kirli" sonuç,
  NEDC + F5, SOC alt/üst, koşullar, devre dışı kaydırıcılar, ayar/CSV dosyalarıyla açılış,
  varsayılana dönme; canlı: oynat, 1 kat, duraklat, devam, Boşluk, sona git, sonuna kadar,
  baştan, F5, oynarken ayar değişimi; karşılaştırma: eski işareti, F5, düğme, strateji
  seçimi, sekme değişimi.
- **CLI ile çapraz kontrol (`gui_karsilastirma_*`, betik `tests/check_gui_compare.cmake`):**
  aynı argümanlarla `hea_cli --strateji hepsi --karsilastir --karsilastir-csv` ve arayüz
  (karşılaştırma sekmesi) çalıştırılır; her strateji için SOC düzeltmeli ve eşdeğer
  L/100 km `%.9g` metin olarak aynı olmalı. Sayılar teste elle yazılmaz. Üç durum:
  sehir_101, NEDC + ornek_ayar.ini, CSV çevrimi.
- **Dışa aktarma (`gui_disa_aktarma_*`, betik `tests/check_gui_export.cmake`):** arayüz
  `--kayit-klasoru` ile Türkçe adlı klasöre adımlar CSV'si, PNG ve karşılaştırma CSV'si
  kaydeder; CSV'ler aynı ayarlarla `hea_cli` çıktısıyla `compare_files` ile bayt bayt
  karşılaştırılır. İki durum (sehir_101 + model, NEDC + ayar dosyası + kural) ve
  yazılamayan klasör (çıkış kodu 4).
- **Testlerin hata yakaladığı, bilerek hata sokularak denendi:** uzun kare sınırı
  kaldırılınca `test_gui_session`; duraklatma etkisizleştirilince 3 canlı senaryo testi;
  eşdeğer yakıtta SOC sırası ters çevrilince 3 karşılaştırma testi; CSV ikili kipte
  (LF) açılınca 2 dışa aktarma testi düştü.
- GUI testlerinin etiketi `gui_ekran`. **OpenGL 3 destekli ekran kartı gerekir**,
  GitHub'ın Windows sanal makinelerinde yoktur. CI'da `-LE gui_ekran` ile hariç tutulmalı.

- **Dağıtım (Adım 6):** `gui_opengl_yok` (`HEA_TEST_OPENGL_SURUMU=9.9`, çıkış kodu 3,
  stderr'de açıklama; `check_gui_png.cmake`'e `EXPECT_ERROR` eklendi) ve
  `paket_dll_bagimliliklari` (etiketsiz, CI'da da çalışır).

Çıkış kodları: 0 başarılı, 2 hatalı argüman, 3 OpenGL hatası (normal modda hata kutusu da),
4 arayüz hata gösteriyor, 5 PNG yazılamadı, 6 eylem uygulanamadı.

## Derleme ve test komutları (bu bilgisayar)

CMake Visual Studio 18 ile geliyor, PATH'te değil:

```
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
%CMAKE%\cmake.exe -S . -B out/gui -G "Visual Studio 18 2026" -A x64 -DHEA_BUILD_GUI=ON -DHEA_BUILD_TESTS=ON
%CMAKE%\cmake.exe --build out/gui --config Release --parallel
%CMAKE%\ctest.exe --test-dir out/gui -C Release --output-on-failure --timeout 60
```

- Program: `out\gui\apps\gui\Release\hea_gui.exe`
- Bağımlılıklar `out/gui/_deps` altında indirilmiş durumda. Yeniden yapılandırırken
  internete gitmemesi için `-DFETCHCONTENT_UPDATES_DISCONNECTED=ON` eklenebilir.
- GUI'siz (CI'ın yaptığı) derleme: aynı komutlar, `-DHEA_BUILD_GUI` olmadan, `out/nogui`.
  Son kontrolde 40/40 geçti.
- Zip: `%CMAKE%\cpack.exe --config out/gui/CPackConfig.cmake -C Release` → `out/gui/paket/`.
- `out/ci-deneme` (sıfırdan CI denemesi) silinebilir; `out/` git'e girmez.

## Adım 2'nin görünür modda denenmesi (13 Kasım 2026) ✅

(Adım 3 ve 5'in görünür denemeleri kendi bölümlerinde.)
Program normal açıldı, düğmelere gerçek fare tıklamasıyla basıldı (PowerShell + Win32;
betik depoda değil, oturum klasöründeydi). Sonuçlar:

1. **Görünür açılış:** 1920x1200 ekranda (çalışma alanı 1920x1152) istemci alanı
   1400x860, çalışma alanının içinde ve tam ortada (fark 0, 0). Bu ekran büyük olduğu için
   küçültme dalı (`min(..., %92 / %88)`) devreye girmedi; küçük ekran yerleşimi yalnızca
   ekran görüntüsü modunda denendi.
2. **"Yükle...":** Windows penceresi "Ayar dosyasını seçin" başlığı ve
   "Ayar dosyası (*.ini)" filtresiyle açıldı. Vazgeç (Esc) → hiçbir şey değişmedi.
   `...\Düğme Denemesi ğüşıöç\ayar_şık.ini` seçildi → Kaynak `ayar_şık.ini`, SOC %45
   (dosyadaki değer), sonuç "eski" olarak işaretlendi.
3. **"CSV dosyasından çevrim ekle...":** "Sürüş çevrimi CSV dosyasını seçin" /
   "Çevrim dosyası (*.csv)" ile açıldı. `çevrim_ığüş.csv` → listeye "Dosya" grubunda
   eklendi ve seçildi (915 s, 14.15 km). Çalıştır → 0.739 L, SOC %45 → %57.8.
4. **Çapraz kontrol:** Aynı iki dosya `--ayar-dosyasi` / `--cevrim-dosyasi` ile ekran
   görüntüsü modunda aynı ayarlarla çalıştırıldı: `yakit_L=0.7386 soc_son=0.5783`,
   birebir aynı.
5. **Kapatma:** Pencere normal yoldan kapatıldı, çıkış kodu 0.

Notlar (görünür testleri otomatikleştirmek için):
- Açma penceresinde UI Automation "Dosya adı" kutusunu ve Aç / İptal düğmelerini beklenen
  kimliklerle göstermedi; kutu Win32 alt penceresi olarak bulundu (`ComboBox` kimlik
  1148 → `Edit`). **Kaydetme** penceresinde ise kutu `Edit` kimlik **1001**, düğmeler
  `Button` kimlik 1 (Kaydet) / 2 (İptal).
- Pencereyi öne getirmek için ALT tuşu hilesi kullanılmamalı: dosya penceresinde menü
  kipini açıp sonraki tuşları yutuyor (sıfır fare hareketi yeterli).
- Var olan bir dosyanın adı yazılınca otomatik tamamlama listesi açılır ve ilk Enter onu
  kabul eder, kaydetmez: "Kaydet" düğmesine `BM_CLICK` (PostMessage) göndermek güvenilir.
- Tuş göndermeden önce hedef pencerenin önde olduğu denetlenmeli (terminale giden bir
  Esc Claude Code'u keser).
- Kullanıcı görünür denemeleri ekrandan izliyor ve diyaloglara tıklayabiliyor: denemeler
  arasız, tek komutta ve kısa yapılmalı.

## Git durumu

- **Hiçbir şey commit edilmedi.** Dal: `main`. GitHub'da (herkese açık depo) etiket ve
  Release yok; son CI çalıştırması 17 Ekim 2026, başarılı.
- Commit bekleyenler (16 Kasım 2026):
  - Yeni: `apps/gui/`, `apps/common/`, `packaging/`, `docs/ARAYUZ_GELISTIRME_NOTLARI.md`,
    `tests/test_gui_session.cpp`, `tests/check_gui_png.cmake`, `tests/check_gui_compare.cmake`,
    `tests/check_gui_export.cmake`, `tests/check_dependencies.cmake`.
  - Değişen: kök `CMakeLists.txt` (`HEA_BUILD_GUI`, `/MT`, `apps/common`, sürüm 0.3.0,
    paket), `apps/cli/CMakeLists.txt` + `apps/cli/csv_export.cpp` (ortak CSV koduna geçiş,
    çıktı bayt bayt aynı), `core/` (yalnızca `std::istream` sürümleri), `tests/CMakeLists.txt`,
    `tests/test_config.cpp`, `.github/workflows/ci.yml`, `README.md` (bu çalışmadan önce de
    değişmişti).
  - Not: `docs/` klasörü git'te hiç yok (izlenen dosya 0). İçindeki `pdf_olustur.py`,
    `hea_cli_ekran.png` ve PDF önceki çalışmadan kalma, commit edilmemişti; PDF 16 Kasım'da
    güncel README'den yeniden üretildi.
- Önceki çalışma `prototip-eksikler` dalı + PR #1 ile birleştirilmişti; aynı yol izlenebilir.

## Açık işler / bilinenler

- **Yüksek DPI:** varsayılan pencere boyutunun ekran ölçeğiyle büyütülmesi gerçek bir
  %125 / %150 ekranda denenmedi (bu bilgisayar %100) → diğer bilgisayarda bakılmalı.
- **Küçük ekran:** pencereyi çalışma alanına sığdırma (küçültme) dalı görünür modda hiç
  devreye girmedi (bu ekran büyük); yerleşim yalnızca ekran görüntüsü modunda denendi.
- **PNG sıkıştırması yok:** bir ekran görüntüsü ~3.6 MB. İstenirse sıkıştırma eklenebilir.
- **Kaydetme penceresi:** karşılaştırma CSV'si gerçek pencereyle kaydedilmedi; uzantısız
  ad var olan dosyayla çakışınca Windows'un sorup sormadığı belirsiz (Adım 5).
- **Deneme artığı:** Adım 5 denemesinde `OneDrive\Belgeler\sehir_101_model-tabanli_adimlar.csv`
  oluştu (proje dışı). Kullanıcı isterse silinecek; kendiliğinden silinmedi.
- **Küçük arayüz ayrıntısı:** "Çalıştır"dan sonra en alttaki yeşil bilgi mesajı (ör.
  "Ayar dosyası yüklendi") yerinde kalıyor; istenirse çalıştırınca silinebilir.
- **Hata kutusu** (OpenGL yok) ekranda görülmedi; yalnızca görünmez modda çıkış kodu ve
  stderr denetlendi. Görünür deneme: `HEA_TEST_OPENGL_SURUMU=9.9` ile `hea_gui.exe`
  çalıştırılır, kutu açılır (kullanıcı onayıyla).
- **İmzasız exe:** diğer bilgisayarda SmartScreen uyarısı beklenir ("Ek bilgi" > "Yine de
  çalıştır"); BENIOKU, README ve Release notunda yazıyor.

## Sonraki plan

### Adım 2 kapanışı ✅
- Dosya düğmeleri ve görünür açılış 13 Kasım 2026'da denendi (yukarıda).

### Adım 3: Canlı simülasyon ✅
- Yukarıda "Yapılanlar" altında.

### Adım 4: Karşılaştırma sekmesi ✅
- Yukarıda "Yapılanlar" altında.

### Adım 5: Dışa aktarma ✅
- Yukarıda "Yapılanlar" altında.

### Adım 6: Dağıtım paketi ve CI ✅ (yerelde)
- Yukarıda "Yapılanlar" altında.

### Adım 7: Gönderme ve diğer bilgisayar (sıradaki)
- GitHub'a gönderme (push, etiket, Release) kullanıcıya sorulmadan yapılmaz.
- Yeni dal (ör. `arayuz`) → commit → push → CI'ı izle (`gh run watch`): `arayuz-paket`
  işinde bağımlılıkların git'ten inmesi, 41 test, zip artifact'ı; `derle-test`'te Node
  uyarısının gitmesi. Artifact'taki zip indirilip denenebilir.
- PR → birleştir → `main` üzerinde `v0.3.0` etiketi → push → `yayinla` işi Release'i açar.
- Diğer bilgisayarda: Releases → zip indir → aç → `hea_gui.exe`. Orada bakılacaklar:
  SmartScreen uyarısı, yüksek DPI (%125 / %150) görünümü, küçük ekranda pencere boyutu.

## Çalışma kuralları

- **Her adım tam doğrulanmadan "bitti" denmez:** sıfır uyarıyla derleme, tüm testler,
  yeni mantık için birim testi, arayüz için ekran görüntüsü + senaryo testi.
- **Kullanıcının ekranında pencere açılmaz.** Doğrulama görünmez ekran görüntüsü
  moduyla yapılır. Görünür pencere gerekiyorsa önce sorulur (Windows dosya pencereleri
  ve gerçek saatle oynatma yalnızca görünür modda denenebiliyor).
- CLI'ın çıktısını değiştirebilecek her değişiklikten önce çıktı saklanır, sonra bayt
  bayt karşılaştırılır.
- Testlerin gerçekten hata yakaladığı, bilerek geçici hata sokularak denenir.
