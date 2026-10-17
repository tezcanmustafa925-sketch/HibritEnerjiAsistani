# Akıllı Hibrit Enerji Asistanı

Hibrit bir aracın enerji yönetimini (motor ne zaman çalışsın, batarya ne zaman
şarj edilsin) simüle eden ve farklı stratejileri **adil biçimde** karşılaştıran bir
C++17 prototipi. Model tabanlı strateji, Python'da eğitilen bir tüketim modelini
kullanarak her an en düşük eşdeğer yakıt maliyetli modu seçer (ECMS).

> Durum: **prototip (v0.2.0)**, terminal uygulaması. Grafik arayüz (Dear ImGui +
> ImPlot) bir sonraki aşamada eklenecek.

## Öne çıkan sonuç

SOC düzeltmeli (SAE J1711 mantığı) karşılaştırma. Fark, kural tabanlı stratejiye göre:

| Strateji | Test çevrimleri (L/100 km) | Fark | NEDC (L/100 km) | Fark |
|---|---:|---:|---:|---:|
| Sadece ICE (rejeneratif fren var) | 4.97 | +4.1% | 4.37 | +22.6% |
| EV öncelikli | 4.96 | +3.9% | 3.70 | +3.9% |
| Kural tabanlı | 4.77 | 0 | 3.57 | 0 |
| **Model tabanlı (ECMS)** | **4.65** | **−2.6%** | **3.38** | **−5.3%** |

Test çevrimleri modelin eğitimde hiç görmediği çevrimlerdir (şehir, otoyol, karma
ve ağır yük + %7 tepe + 0 °C koşulu). Yeniden üretmek için:

```bash
hea_cli --cevrim test --strateji hepsi --karsilastir
```

## Hızlı başlangıç

Gereken: CMake ≥ 3.16 ve C++17 derleyici (MSVC 2019+, GCC 9+, Clang 10+).
Model eğitimi için ayrıca Python ≥ 3.9.

```bash
cmake --preset varsayilan
cmake --build --preset varsayilan
ctest --preset varsayilan
```

Visual Studio'da klasörü açmak da yeterlidir (CMake projesi olarak tanınır).
Çalıştırılabilir dosya: `build/apps/cli/Release/hea_cli` (Windows'ta `.exe`).

```bash
# Varsayılan: sehir_101 çevrimi, model tabanlı strateji, canlı akış (10 kat hızlı)
hea_cli

# Dört strateji, bir çevrim, yalnızca özet tablo
hea_cli --cevrim nedc --strateji hepsi --ozet

# Kendi çevriminiz (CSV) ve parametreleriniz
hea_cli --cevrim-dosyasi data/cevrimler/ornek_tepe_yolu.csv --strateji hepsi \
        --ayar batarya.kapasite_Ah=8 --soc0 0.5 --ozet --ozet-csv sonuc.csv

# Tüm seçenekler
hea_cli --yardim
```

## Mimari

```
core/        hea_core statik kütüphanesi: fizik, stratejiler, simülasyon (ekrana yazmaz)
apps/cli/    hea_cli terminal uygulaması: tablolar, canlı akış, CSV
model/       Python: tüketim modeli eğitimi ve katsayıların C++'a aktarılması
tests/       Birim testleri + CLI testleri + Python uçtan uca testi (CTest)
data/        Örnek ayar dosyası ve örnek çevrim CSV'si
```

Bir simülasyon adımının akışı (geriye dönük model, ideal sürücü):

```
DriveCycle ──► VehicleModel ──► Strategy.decide ──► Powertrain.resolve ──► Battery.step
(hedef hız,    (yol yükü →       (EV mi ICE mi,      (tork paylaşımı,       (SOC, V, I,
 eğim, yük,     tekerlek torku)   şarj gücü)          vites, yakıt, rejen)   sıcaklık)
 sıcaklık)                                                                      │
                                         StepResult (her adım, SI) ◄────────────┘
```

| Bileşen | Model |
|---|---|
| Araç | Boylamasına dinamik: aero + yuvarlanma + eğim + atalet (dönen kütle katsayısı) |
| Batarya | Thevenin 1-RC eşdeğer devre, OCV(SOC) tablosu, sıcaklığa bağlı direnç, toplu ısıl model |
| Elektrik motoru | Analitik kayıp haritası (bakır, demir, rüzgâr, sabit kayıp), tork/güç sınırı |
| İçten yanmalı motor | Willans hattı yakıt modeli, tam yük eğrisi, **soğuk motor sürtünmesi + ısıl model** |
| Aktarma | Paralel hibrit: ICE → 5 vites → diferansiyel; EM → sabit redüksiyon → aks |
| Yardımcı yük | Sabit 300 W + **ortam sıcaklığına bağlı klima/ısıtma yükü** |

### Stratejiler

| Ad (`--strateji`) | Açıklama |
|---|---|
| `ice` | Sadece içten yanmalı motor (rejeneratif fren hariç elektrik kullanmaz) |
| `ev` | EV öncelikli: yetmeyince güç aktarma organı ICE'ye geçer, şarj etmez |
| `kural` | Hız/güç/SOC eşikleri + histerezis + asgari motor çalışma süresi + yük noktası kaydırma |
| `model` | ECMS: EV, ICE ve ICE+şarj seçeneklerinin eşdeğer yakıt maliyeti, SOC geri beslemeli eşdeğerlik katsayısı |

### Adil karşılaştırma

Stratejiler çevrimi farklı SOC ile bitirir; ham L/100 km bataryayı boşaltanı ödüllendirir.
`--karsilastir` her stratejiyi 9 farklı başlangıç SOC'si ile koşturur ve batarya
enerjisinin net değişmediği noktadaki yakıtı doğrusal ara değerle bulur. Kontrol
yöntemi olarak eşdeğer yakıt (k = 2.8 / LHV) da gösterilir.

## Tüketim modeli (Python)

Model tabanlı strateji, simülasyonun "ne olurdu" (karşı-olgusal) çözümlerinden
öğrenilen üç lineer modeli kullanır: EV batarya gücü, ICE yakıt debisi ve şarjın
marjinal yakıt maliyeti. Katsayılar elle kopyalanmaz, betikle C++ başlığına aktarılır.

```bash
pip install -r model/requirements.txt
hea_cli --cevrim egitim --egitim-verisi build/veri/egitim.csv
hea_cli --cevrim test   --egitim-verisi build/veri/test.csv
python model/egit.py            # model/tuketim_modeli.json + build/model/*.png
python model/cpp_aktar.py       # core/include/hea/consumption_model_coeffs.h
```

`--egitim-surucu hepsi` eğitim verisini kural, model ve EV öncelikli sürüşlerle
birlikte üretir (daha çeşitli durumlar).

Testler şunları otomatik denetler: model doğruluğu eşiklerin üstünde mi, C++ başlığı
JSON ile aynı mı, **depodaki katsayılar güncel fizikle uyumlu mu** (fizik değişip
model yeniden eğitilmezse `py_model_egitimi` testi düşer).

## Parametreler

Bütün parametreler (araç, batarya, motorlar, aktarma, klima, iki stratejinin
eşikleri) bir INI dosyasından ya da komut satırından değiştirilebilir:

```bash
hea_cli --ayarlari-yaz benim_aracim.ini      # tüm anahtarlar, açıklamalar, varsayılanlar
hea_cli --ayar-dosyasi benim_aracim.ini --cevrim hepsi --strateji hepsi --ozet
hea_cli --ayar klima.acik=hayir --ayar simulasyon.soguk_baslangic=hayir --ozet
```

Örnek: [data/ornek_ayar.ini](data/ornek_ayar.ini). Geçersiz bir değer (ör. SOC
penceresi dışında başlangıç SOC'si) simülasyon başlamadan açıklayıcı bir hatayla reddedilir.

## Sürüş çevrimleri

| Grup (`--cevrim`) | İçerik |
|---|---|
| `egitim` | 3 şehir, 2 otoyol, 2 karma çevrim (sentetik, tohumlu, tekrarlanabilir) |
| `test` | Eğitimdekinden farklı tohumlarla 4 çevrim; biri eğitimde olmayan zorlu koşul |
| `standart` | NEDC (4 × ECE-15 + EUDC, 1180 s, ~10.9 km) |
| `hepsi` | Yukarıdakilerin tümü |

Kendi çevriminiz için CSV: `time_s,speed_kmh[,grade_percent][,payload_kg][,ambient_C]`.
Zaman aralıkları düzensiz olabilir, 1 s'ye yeniden örneklenir. Örnek:
[data/cevrimler/ornek_tepe_yolu.csv](data/cevrimler/ornek_tepe_yolu.csv).

## Çıktılar

| Seçenek | İçerik |
|---|---|
| (varsayılan) | Adım adım tablo: hız, gaz/fren, mod, vites, devir/tork, batarya V/I/T, SOC, yakıt |
| `--csv` | Her adımın tüm alanları (SI birimleri), Python/Excel analizi için |
| `--ozet-csv` | Her koşunun özeti |
| `--karsilastir-csv` | SOC düzeltmeli karşılaştırma sonuçları |
| `--tablolar` | Bileşen kontrol tabloları (yol yükü, BSFC haritası, EM verimi) |

Çıkış kodları: 0 başarılı, 1 dosya hatası, 2 hatalı kullanım/ayar, 3 çalışma hatası.

## Varsayımlar ve bilinen sınırlar

Prototipte bilerek basit tutulan ya da henüz olmayanlar:

- **Mimari:** Yalnızca paralel hibrit. Prius gibi güç bölücülü (planet dişli) ve seri
  hibritler modellenmiyor; bileşen boyutları o sınıfa yakın.
- **Sürücü:** İdeal sürücü, hedef hızı gücün yettiği her durumda tam izler (geriye
  dönük model). İleriye dönük sürücü modeli (PID) yok.
- **Vites:** Her adımda en düşük yakıtlı vites anında seçilir; vites değiştirme süresi
  ve histerezisi yok.
- **Motor freni** ve yakıt kesmede motorun sürüklenme direnci modellenmiyor.
- **Klima:** Ortam sıcaklığına bağlı doğrusal model; güneş yükü ve kabin ısıl ataleti yok.
- **Batarya:** Tek RC dalı; yaşlanma ve hücre dengesizliği yok.
- **Şarj maliyeti modeli** tek tek örneklerde zayıf (R² ≈ 0.31): maliyet vites
  değişimlerinde basamaklı sıçrıyor ve polinom özellikler bunu yakalayamıyor. Çevrim
  toplamında hata %1'in altında olduğu için strateji için şimdilik yeterli. Yalnızca
  Python'da duran doğrusal olmayan kıyas modeli (GBR) R² ≈ 0.75'e çıkıyor; vitese
  göre parçalı bir model ya da küçük bir ağaç modeli C++'a taşınarak iyileştirilebilir.
- **Soğuk motor:** Tüketim modeli sıcak motoru temsil eder; ECMS soğuk motorun fazladan
  yakıtını maliyet hesabına katmaz (simülasyon katar).
- **Duruşta düşük batarya:** Araç dururken SOC alt sınırdaysa motor şarj için
  çalıştırılmaz; karşılanamayan yardımcı yük raporlanır (özet tabloda UYARI).
- Sentetik çevrimler gerçek trafik verisi değildir; WLTP gibi tablo tabanlı
  çevrimler CSV olarak eklenebilir.

## Yol haritası

1. Grafik arayüz (Dear ImGui + ImPlot): canlı grafikler, parametre sürgüleri, strateji karşılaştırma
2. Şarj maliyeti modelinin iyileştirilmesi (vitese göre parçalı model)
3. Güç bölücülü (power-split) hibrit mimarisi
4. Öngörülü strateji (rota/eğim bilgisiyle), WLTP çevrimi
