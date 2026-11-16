# Akıllı Hibrit Enerji Asistanı

Hibrit bir aracın enerji yönetimini (motor ne zaman çalışsın, batarya ne zaman
şarj edilsin) simüle eden ve farklı stratejileri **adil biçimde** karşılaştıran bir
C++17 prototipi. Model tabanlı strateji, Python'da eğitilen bir tüketim modelini
kullanarak her an en düşük eşdeğer yakıt maliyetli modu seçer (ECMS).

> Durum: **prototip (v0.3.0)**: grafik arayüz (`hea_gui`, Dear ImGui + ImPlot) ve
> terminal uygulaması (`hea_cli`).

**Windows'ta hemen denemek için:**
[Releases](https://github.com/tezcanmustafa925-sketch/HibritEnerjiAsistani/releases)
sayfasından `HibritEnerjiAsistani-<sürüm>-windows-x64.zip` dosyasını indirin, bir klasöre
çıkarın ve `hea_gui.exe`'ye çift tıklayın. Kurulum gerekmez (ayrıntı: aşağıda
"Grafik arayüz" bölümü).

Bu belgenin PDF hâli: [docs/HibritEnerjiAsistani_Rehber.pdf](docs/HibritEnerjiAsistani_Rehber.pdf)
(yeniden üretmek için: `python docs/pdf_olustur.py`).

## Bu proje ne işe yarar? (Konuya yabancı olanlar için)

### Hibrit araç nedir?

Hibrit araçta iki ayrı "kalp" vardır:

- **Benzinli motor** (kısaca **ICE**, *internal combustion engine*): Bildiğimiz motor.
  Yakıt yakar, güçlüdür, ama özellikle düşük hızda ve dur-kalk trafikte verimsizdir.
- **Elektrik motoru** (kısaca **EM**) ve onu besleyen **batarya**: Sessizdir, düşük
  hızda çok verimlidir, ama batarya küçüktür; uzun süre tek başına aracı götüremez.

Elektrik motoru ters yönde de çalışabilir: araç yavaşlarken tekerleklerin dönüşünü
elektriğe çevirip bataryayı doldurur. Buna **rejeneratif fren** (geri kazanımlı fren)
denir. Normal araçta frenlerken o enerji ısı olarak boşa gider.

### Asıl soru: hangisi, ne zaman?

Araç yoldayken her saniye bir karar verilmesi gerekir:

- Şimdi yalnızca elektrikle mi gidelim? (Yakıt harcanmaz ama batarya boşalır.)
- Benzinli motoru mu çalıştıralım? (Yakıt harcanır.)
- Motor zaten çalışıyorsa biraz fazla çalıştırıp bataryayı da mı dolduralım?
  (Şimdi biraz fazla yakıt, sonra bedava elektrik.)

Bu kararları veren "beyne" **enerji yönetim stratejisi** denir. İyi bir strateji aynı
yolu daha az yakıtla gider. Bu proje, bir bilgisayarın içinde sanal bir hibrit araç
kurar, onu sanal bir yolda sürer ve farklı stratejilerin ne kadar yakıt harcadığını
ölçer. Gerçek araç ya da test pisti gerekmez; her şey fizik formülleriyle hesaplanır.
Buna **simülasyon** denir.

### Karşılaştırılan dört strateji (dört "sürücü beyni")

| Ekrandaki adı | Basitçe ne yapar? |
|---|---|
| `sadece-ICE` | Hep benzinli motorla gider; elektriği yalnızca frende geri kazanır. Kıyas için "en kötü durum". |
| `EV-oncelikli` | Mümkün olduğunca elektrikle gider, batarya bitince mecburen motora geçer. Bataryayı hiç bilerek doldurmaz. |
| `kural-tabanli` | Mühendisin elle yazdığı kurallar: "hız şu kadardan azsa ve batarya doluysa elektrikle git" gibi. Bugünkü birçok aracın yaklaşımı. |
| `model-tabanli` | Projenin asıl katkısı. Her saniye üç seçeneğin "maliyetini" hesaplar (elektrik, motor, motor + şarj) ve en ucuzunu seçer. Maliyeti tahmin etmek için, simülasyon verisiyle Python'da eğitilmiş bir tüketim modeli kullanır. Teknik adı ECMS. |

Kısaca sonuç: model tabanlı strateji, kural tabanlıya göre **%2.6-5.3 daha az yakıt**
harcıyor (aşağıdaki "Öne çıkan sonuç" tablosu).

### "Adil karşılaştırma" neden gerekli?

Bir strateji yolculuk boyunca bataryayı boşaltırsa daha az yakıt harcamış *görünür*;
ama o boşalan elektrik bir gün yakıtla geri doldurulacaktır. Bu "hileyi" ayıklamak
için her strateji farklı batarya doluluklarıyla tekrar tekrar çalıştırılır ve
**bataryanın başta ve sonda aynı dolulukta olduğu** durumdaki yakıt hesaplanır
(`--karsilastir`). Böylece hepsi aynı şartlarda yarışmış olur.

### Program nasıl çalışır? (Bir saniyenin hikâyesi)

Simülasyon yolu 1 saniyelik adımlara böler. Her adımda sırasıyla:

1. **Sürüş çevrimi** "şu anda 40 km/h hızda olmalısın, yol %1 yokuş, araçta 165 kg
   yük var, hava 19 °C" der. (Çevrim = önceden belirlenmiş hız-zaman programı;
   gerçek trafiği taklit eder.)
2. **Araç modeli** bu hızı tutmak için tekerleğe ne kadar kuvvet gerektiğini
   hesaplar: hava direnci, lastik direnci, yokuş ve hızlanma.
3. **Strateji** karar verir: elektrik mi, motor mu, şarj edilsin mi?
4. **Güç aktarma organı** kararı uygular: vitesi seçer, motorun devrini ve torkunu,
   harcanan yakıtı, elektrik motorunun çektiği akımı hesaplar.
5. **Batarya modeli** bu akımla bataryanın gerilimini, doluluğunu ve sıcaklığını
   günceller.
6. Sonuçlar ekrana bir satır olarak yazılır ve bir sonraki saniyeye geçilir.

## Ekrandaki değerler ne anlama geliyor?

Program varsayılan olarak (`hea_cli`) `sehir_101` adlı şehir içi yolu model tabanlı
stratejiyle "canlı" sürer: her satır aracın bir saniyelik durumudur ve satırlar
gerçek zamandan 10 kat hızlı akar. Örnek:

```
      t  hedef    hiz  egim  gaz fren mod     v   ICE   ICE    EM    EM    bat    bat   bat   SOC  yakit   yakit
    [s] [km/h] [km/h]   [%]  [%]  [%]           [rpm]  [Nm] [rpm]  [Nm]    [V]    [A]   [C]   [%]  [g/s]  top[g]
     20   40.0   40.0  -0.2   19    0 ICE     3  1833 112.6  2686   0.0  210.8    1.4  19.1  59.6  1.399     7.1
     40   40.0   40.0  -0.9    0    0 EV      0     0   0.0  2828   1.6  209.1    6.1  19.1  59.4  0.000     9.3
     80   16.8   16.8  -0.9    0   14 REGEN   0     0   0.0  1337 -68.6  225.4  -36.6  19.3  60.5  0.000     9.3
    100    0.0    0.0  -0.8    0    0 DUR     0     0   0.0     0   0.0  211.7    1.4  19.3  60.7  0.000     9.3
```

Başlığın ilk satırı değerin adı, ikinci satırı birimidir. Her sütun:

| Sütun | Birim | Ne demek? | Nereden geliyor? |
|---|---|---|---|
| `t` | s | Yolculuğun başından beri geçen süre. | Simülasyon saati; her adımda 1 s artar. |
| `hedef` | km/h | Sürüş çevriminin "bu anda bu hızda ol" dediği hız. | Çevrim verisi (yerleşik sentetik çevrimler, NEDC ya da `--cevrim-dosyasi` ile verilen CSV). |
| `hiz` | km/h | Aracın gerçekte ulaştığı hız. Normalde `hedef` ile aynıdır; araç gücü yetmezse (dik yokuş, ağır yük) geride kalır. | Araç modeli: tekerleğe verilebilen kuvvetle hesaplanır. |
| `egim` | % | Yolun eğimi. Artı = yokuş yukarı, eksi = yokuş aşağı. %5, 100 m'de 5 m yükselmek demektir. | Çevrim verisi. |
| `gaz` | % | Sanal gaz pedalı: istenen çekiş torkunun, aracın o hızda verebileceği en büyük torka oranı. | Hesaplanan bir gösterge (gerçek bir sürücü pedalı yoktur). |
| `fren` | % | Sanal fren pedalı: istenen fren kuvvetinin lastiğin yola aktarabileceği en büyük kuvvete oranı. | Hesaplanan gösterge. |
| `mod` | - | Aracın bu saniyedeki çalışma şekli (aşağıdaki tabloya bakın). | Stratejinin kararı + simülasyon. |
| `v` | - | Vites (1-5). Benzinli motor kapalıysa 0. | Her adımda en az yakıt harcatan vites seçilir. |
| `ICE [rpm]` | dev/dak | Benzinli motorun devri (dakikadaki dönüş sayısı). Kapalıysa 0. | Araç hızı × vites oranı × diferansiyel oranı. |
| `ICE [Nm]` | Nm | Benzinli motorun ürettiği tork (döndürme kuvveti). | Güç aktarma organı: tekerlek isteği + (varsa) şarj isteği. |
| `EM [rpm]` | dev/dak | Elektrik motorunun devri. Tekerleklere sabit dişliyle bağlı olduğundan araç hareket ettikçe hep döner, yük taşımasa bile. | Araç hızı × sabit dişli oranı. |
| `EM [Nm]` | Nm | Elektrik motorunun torku. **Artı**: aracı itiyor (batarya boşalıyor). **Eksi**: jeneratör gibi çalışıp bataryayı dolduruyor (frenleme ya da motorla şarj). | Güç aktarma organı. |
| `bat [V]` | volt | Bataryanın uçlarındaki gerilim. Yaklaşık 210 V civarındadır; çekilen akım artınca düşer, şarjda yükselir. | Batarya modeli: açık devre gerilimi (doluluğa bağlı tablo) ve iç direnç kaybı. |
| `bat [A]` | amper | Bataryadan akan akım. **Artı**: boşalıyor. **Eksi**: şarj oluyor. Araç dururken bile ~1.4 A görülür; bu, lambalar, kontrol üniteleri ve klimanın sürekli çektiği yüktür. | Batarya modeli: istenen güç ÷ gerilim. |
| `bat [C]` | °C | Bataryanın sıcaklığı. Ortam sıcaklığında başlar, kullanıldıkça ısınır. | Batarya ısıl modeli: iç kayıplar ısıtır, ortam soğutur. |
| `SOC` | % | *State of Charge*, bataryanın doluluk oranı (telefondaki pil yüzdesi gibi). Batarya ömrü için %30-80 arasında tutulur; yolculuk %60 ile başlar. | Batarya modeli: akan akım zamanla toplanır. |
| `yakit [g/s]` | gram/saniye | O anda harcanan benzin miktarı. Motor kapalıysa 0. | Benzinli motorun yakıt modeli (devir ve torka bağlı). Soğuk motor daha çok yakar. |
| `yakit top[g]` | gram | Yolculuk başından beri harcanan toplam benzin. 1 litre benzin ≈ 745 g. Motorun her çalıştırılışında küçük bir ek yakıt (0.2 g) eklenir. | Anlık yakıtın zamanla toplamı. |

### Mod (çalışma şekli) değerleri

| Ekranda | Anlamı |
|---|---|
| `EV` | Araç yalnızca elektrikle gidiyor; benzinli motor kapalı, yakıt harcanmıyor. |
| `ICE` | Benzinli motor aracı itiyor. Bu sırada `EM [Nm]` eksiyse motor aynı anda bataryayı da dolduruyor. |
| `REGEN` | Araç yavaşlıyor ya da yokuş iniyor; elektrik motoru frenleme enerjisini bataryaya geri kazanıyor (`bat [A]` eksi olur). |
| `DUR` | Araç duruyor (trafik ışığı gibi). Yalnızca yardımcı yükler bataryadan çekilir. |
| `*` (ör. `ICE*`) | Strateji başka bir şey istedi ama uygulanamadı. Örneğin elektrik istedi, fakat batarya ya da elektrik motoru o kadar gücü veremediği için motor çalıştırıldı. |

`EV` ve `ICE` kararlarını **strateji** verir; `REGEN` ve `DUR` durumlarını aracın o
anki hareketine bakarak simülasyon kendisi belirler.

### Yolculuk sonunda: ÖZET tablosu

Her koşunun sonunda tek satırlık bir özet yazılır (`--ozet` ile yalnızca bu tablo gelir):

```
cevrim          strateji          yol  L/100km   yakit   SOC0   SOC1    dSOC    EV   ICE   REG   DUR calis.  rejen uyg.yok
                                 [km]              [L]    [%]    [%]     [%]   [%]   [%]   [%]   [%] [adet]   [Wh]    [%]
nedc            kural-tabanli   10.89     3.80   0.414   60.0   66.0    +6.0  35.1  25.4  15.8  23.7      7    364    0.0
nedc            model-tabanli   10.89     3.69   0.402   60.0   67.8    +7.8  33.2  27.3  15.8  23.7     24    364    0.0
```

| Sütun | Anlamı |
|---|---|
| `cevrim` | Sürülen yolun (sürüş çevriminin) adı. |
| `strateji` | Kararları veren strateji. |
| `yol [km]` | Toplam gidilen mesafe. |
| `L/100km` | 100 km'de kaç litre benzin harcandığı (arabaların bilinen yakıt tüketimi ölçüsü). **Küçük olan iyidir.** |
| `yakit [L]` | Bu yolculukta harcanan toplam benzin (litre). |
| `SOC0` / `SOC1` | Yolculuk başındaki ve sonundaki batarya doluluğu. |
| `dSOC` | Doluluktaki değişim (`SOC1 − SOC0`). Eksi: batarya boşaltılmış; artı: doldurulmuş. Bu sütun sıfırdan uzaksa ham `L/100km` yanıltıcı olabilir (bkz. adil karşılaştırma). |
| `EV` / `ICE` / `REG` / `DUR` | Sürenin yüzde kaçının o modda geçtiği. |
| `calis. [adet]` | Benzinli motorun kaç kez çalıştırıldığı. Çok sık çalıştırma yakıt ve konfor açısından kötüdür. |
| `rejen [Wh]` | Frenlemeden bataryaya geri kazanılan enerji (watt-saat). |
| `uyg.yok [%]` | Stratejinin isteğinin uygulanamadığı sürüş anlarının oranı (ekrandaki `*` işaretleri). Yüksekse strateji gerçekçi olmayan şeyler istiyor demektir. |

Altta `UYARI` satırı çıkarsa: batarya en alt sınırdayken araç durduğu için elektrik
yükü (lambalar, klima) karşılanamamış demektir.

### Adil karşılaştırma tablosu (`--karsilastir`)

```
cevrim          strateji          ham   dSOC  DUZELT.     R2       b     fark esdeger     fark
                              L/100km    [%]  L/100km        [g/kWh]      [%] L/100km      [%]
nedc            kural-tabanli    3.80   +6.0     3.57  0.999     217     +0.0    3.56     +0.0
nedc            model-tabanli    3.69   +7.8     3.37  0.999     212     -5.3    3.37     -5.1
```

| Sütun | Anlamı |
|---|---|
| `ham L/100km` | Özet tablodaki gibi, düzeltme yapılmamış tüketim. |
| `dSOC [%]` | Ham koşuda bataryanın doluluk değişimi. |
| `DUZELT. L/100km` | **Asıl sonuç.** Batarya başta ve sonda aynı dolulukta olsaydı harcanacak yakıt. Strateji 9 farklı başlangıç doluluğuyla (%40-78) koşturulur ve "batarya değişmedi" noktası ara değerle bulunur. Yanında `*` varsa bu noktaya ulaşılamamış, tahmin dışarıya uzatılmıştır (daha az güvenilir). |
| `R2` | 9 koşunun bir doğruya ne kadar iyi oturduğu (1'e yakınsa yöntem güvenilir). |
| `b [g/kWh]` | Bataryaya 1 kWh enerji koymanın bu stratejide kaç gram yakıta mal olduğu. |
| `fark [%]` | `kural-tabanli` stratejisine göre fark. **Eksi = daha az yakıt = daha iyi.** |
| `esdeger L/100km` / `fark` | Kontrol amaçlı ikinci yöntem: batarya değişimi sabit bir katsayıyla (1 kWh ≈ 234 g benzin) yakıta çevrilip eklenir. İki yöntem birbirine yakın çıkmalıdır. |

En alttaki `TOPLAM` tablosu, seçilen bütün çevrimleri mesafeye göre ağırlıklı
birleştirir; `dis.d.` sütunu kaç çevrimde dışarıya uzatma (`*`) gerektiğini sayar.

### Çevrim listesi (`--cevrimler`)

| Sütun | Anlamı |
|---|---|
| `sure [s]`, `yol [km]` | Çevrimin süresi ve uzunluğu. |
| `ort km/h`, `max km/h` | Ortalama ve en yüksek hız. |
| `dur [%]` | Sürenin yüzde kaçında aracın durduğu (şehir içinde yüksek). |
| `egim+`, `egim-` | En dik yokuş yukarı ve yokuş aşağı eğim. |
| `yuk [kg]` | Araçtaki ek yük (yolcu + bagaj). |
| `T [C]` | Ortam (hava) sıcaklığı. Soğuk havada motor geç ısınır ve ısıtıcı çalışır; sıcakta klima çalışır. İkisi de tüketimi artırır. |

### Bu sayılar nereden geliyor? Gerçek mi?

- **Hiçbir değer bir sensörden ya da gerçek araçtan okunmuyor.** Hepsi fizik
  formülleriyle hesaplanıyor. Aracın özellikleri (1380 kg kütle, 6.5 Ah / ~210 V
  batarya, 53 kW elektrik motoru, 5 vitesli benzinli motor) Toyota Prius sınıfı
  kompakt bir hibrite yakın seçilmiştir. Hepsi bir ayar dosyasıyla değiştirilebilir
  (aşağıda "Parametreler").
- **Yollar (çevrimler)** ya program içinde rastgele ama her seferinde aynı üretilen
  sentetik şehir/otoyol/karma yollardır, ya Avrupa'nın resmi yakıt testi olan
  **NEDC**'dir, ya da kullanıcının verdiği bir CSV dosyasıdır.
- **Model tabanlı stratejinin bilgisi**, simülasyonun ürettiği eğitim verisinden
  Python'da öğrenilmiş katsayılardır. Strateji, eğitimde **hiç görmediği** test
  yollarında da denenir; böylece ezber yapmadığı gösterilir.
- Tam sayılar hesaplama sırasına göre ondalıklarda küçük farklar gösterebilir;
  ama aynı ayarlarla program her çalıştırmada aynı sonucu verir.

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
Model eğitimi için ayrıca Python ≥ 3.9. Aşağıdaki komutlar terminal uygulamasını
derler; grafik arayüz için "Grafik arayüz" bölümündeki komutlar kullanılır.

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

## Grafik arayüz

### Hazır paket (Windows, kurulumsuz)

[Releases](https://github.com/tezcanmustafa925-sketch/HibritEnerjiAsistani/releases)
sayfasındaki zip dosyasının içeriği:

| Dosya | İçerik |
|---|---|
| `hea_gui.exe` | Grafik arayüz |
| `hea_cli.exe` | Terminal uygulaması (OpenGL gerektirmez) |
| `data/` | Örnek ayar dosyası ve örnek çevrim CSV'si |
| `BENIOKU.txt` | Kısa kullanım notu |
| `lisanslar/` | Exe'ye gömülü açık kaynak bileşenlerin lisansları |

Her şey exe'ye statik bağlıdır: Visual C++ Redistributable, DLL ya da yazı tipi kurmak
gerekmez. Exe'ler yalnızca her Windows'ta bulunan sistem DLL'lerini kullanır; bunu bir
test (`paket_dll_bagimliliklari`) denetler. Gerekenler: Windows 10/11 (64 bit) ve OpenGL
3.0 destekli ekran kartı sürücüsü. OpenGL 3 yoksa (eski sürücü, Uzak Masaüstü, sanal
makine) program nedenini bir hata kutusuyla bildirir. Program imzalı olmadığı için Windows
ilk açılışta "kişisel bilgisayarınızı korudu" uyarısı gösterebilir: "Ek bilgi" > "Yine de
çalıştır".

Paket, `v` ile başlayan bir etiket gönderildiğinde (ör. `v0.3.0`) CI tarafından
derlenip test edilir ve Releases sayfasına yüklenir. Her push'ta üretilen zip, CI
çalıştırmasının "Artifacts" bölümünden de indirilebilir.

### Ekranda neler var?

- **Sol panel:** çevrim seçimi (hazır çevrimler ve "CSV dosyasından çevrim ekle..."),
  strateji ve açıklaması, başlangıç SOC'si, sabit ortam sıcaklığı, sabit yük, klima,
  motor soğuk başlangıç, ayar dosyası ("Yükle...", "Varsayılana dön"). Ayar değişince
  önceki sonuç "eski" olarak işaretlenir.
- **Sürüş sekmesi:** ana sonuçlar (tüketim, toplam yakıt, SOC baş/son, elektrikle geçen
  süre, motor çalıştırma sayısı); hız, mod şeridi (Elektrikle / Motorla / Rejeneratif fren /
  Duruş), SOC ve toplam yakıt grafikleri.
- **Canlı oynatma:** simülasyon hızlandırılmış gerçek zamanda ilerler, grafikler soldan
  sağa dolar; anlık hız, motor devri, SOC, mod ve yakıt gösterilir. Hız 1-100 kat
  (10 kat = `hea_cli --hiz 10`).
- **Strateji karşılaştırması sekmesi:** dört strateji, ham ve SOC düzeltmeli tüketim
  (çubuk grafik ve tablo); hesap `hea_cli --karsilastir` ile aynıdır.
- **Dışa aktarma:** "CSV olarak kaydet" (adım sonuçları ya da karşılaştırma; dosya
  `hea_cli --csv` / `--karsilastir-csv` çıktısıyla bayt bayt aynı) ve "Görüntü kaydet" (PNG).

| Tuş | İşlev |
|---|---|
| F5 | Çalıştır (Sürüş sekmesi) / Karşılaştır (karşılaştırma sekmesi) |
| Boşluk | Canlı oynatmayı duraklat / sürdür |

### Kaynaktan derleme

Arayüz isteğe bağlıdır (`HEA_BUILD_GUI`, varsayılan kapalı). Dear ImGui 1.91.8,
ImPlot 0.16 ve GLFW 3.4 ilk yapılandırmada git ile indirilir (internet ve git gerekir):

```bash
cmake -S . -B out/gui -DHEA_BUILD_GUI=ON -DHEA_BUILD_TESTS=ON
cmake --build out/gui --config Release --parallel
ctest --test-dir out/gui -C Release --output-on-failure
cpack --config out/gui/CPackConfig.cmake -C Release    # zip: out/gui/paket/
```

Arayüz testleri (`gui_ekran` etiketi) programı görünmez pencerede çizip ekran görüntüsü
alır, fare ve klavye olaylarını taklit eder, sonuçları `hea_cli` çıktısıyla karşılaştırır.
OpenGL 3 destekli ekran kartı gerektirdikleri için CI'da `-LE gui_ekran` ile hariç tutulur.

### Komut satırı (`hea_gui`)

Tüm seçenekler isteğe bağlıdır:

| Seçenek | Anlamı |
|---|---|
| `--cevrim <ad>` | Açılışta seçili hazır çevrim (ör. `nedc`) |
| `--strateji <ad>` | `ice`, `ev`, `kural` ya da `model` |
| `--cevrim-dosyasi <csv>` | CSV çevrimi ekle ve seç |
| `--ayar-dosyasi <ini>` | Ayar dosyasını yükle |
| `--boyut <G>x<Y>` | Pencere boyutu (verilmezse 1400x860, ekrana sığdırılır) |
| `--olcek <k>` | Yazı ve arayüz ölçeği (verilmezse ekranın DPI ölçeği) |
| `--kayit-klasoru <klasör>` | Kaydet düğmeleri dosya penceresi açmadan bu klasöre yazar |
| `--ekran-goruntusu <png>` | Pencere göstermeden çiz, PNG'ye yaz, durum satırını bas ve çık (testler için) |
| `--eylem <eylem>`, `--kare <n>` | Ekran görüntüsünden önce fare/klavye olayı taklit et (testler için) |

Çıkış kodları: 0 başarılı, 2 hatalı argüman, 3 pencere/OpenGL başlatılamadı, 4 arayüz
hata gösteriyor, 5 PNG yazılamadı, 6 eylem uygulanamadı.

## Mimari

```
core/         hea_core statik kütüphanesi: fizik, stratejiler, simülasyon (ekrana yazmaz)
apps/common/  hea_result_csv: adım ve karşılaştırma CSV satırları (CLI ve arayüz ortak)
apps/cli/     hea_cli terminal uygulaması: tablolar, canlı akış, CSV
apps/gui/     hea_gui grafik arayüz: Dear ImGui + ImPlot, GLFW + OpenGL 3
model/        Python: tüketim modeli eğitimi ve katsayıların C++'a aktarılması
tests/        Birim, CLI ve arayüz senaryo testleri + Python uçtan uca testi (CTest)
data/         Örnek ayar dosyası ve örnek çevrim CSV'si
packaging/    Dağıtım paketi (zip): BENIOKU.txt ve lisans metinleri
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

1. Şarj maliyeti modelinin iyileştirilmesi (vitese göre parçalı model)
2. Güç bölücülü (power-split) hibrit mimarisi
3. Öngörülü strateji (rota/eğim bilgisiyle), WLTP çevrimi
