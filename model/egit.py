"""Tüketim tahmin modeli eğitimi.

Girdi : hea_cli --egitim-verisi ile üretilen CSV dosyaları (eğitim ve test).
Çıktı : model/tuketim_modeli.json  (C++'a taşınacak katsayılar)
        build/model/*.png          (tahmin-gerçek grafikleri)

Üç lineer model eğitilir. Hepsi stratejinin C++'ta zaten bildiği büyüklüklerden
(tekerlek gücü P, torku T, açısal hızı w) hesaplanan özellikleri kullanır:

1) EV batarya gücü [W] — EV seçilseydi bataryadan çekilecek güç.
   Fizik (3b. adım): P_e = T_m*w_m + k_c*T_m^2 + k_i*w_m + k_w*w_m^3 + C,
   T_m = T/(G*eta), w_m = G*w  ->  P_e, [P, T^2, w, w^3]'ün doğrusal birleşimidir.

2) ICE yakıt debisi [kg/s] — ICE seçilseydi (şarjsız) yakıt debisi.
   Fizik: P_yakit = (P/eta + a*w_e + b*w_e^2) / e. Motor devri w_e seçilen vitese
   bağlı olduğu için ilişki doğrusal değildir; [P, w, w^2, T, T^2] ile ikinci
   dereceden bir yaklaşım kullanılır (T*w yok, çünkü P = T*w).

3) Şarjın marjinal yakıt maliyeti [kg/J] — ICE modunda bataryaya verilen her
   joule için harcanan ek yakıt: (yakit_sarjli - yakit) / P_sarj.

Lineer modelin yeterli olup olmadığını görmek için, yalnızca Python'da kalan
doğrusal olmayan bir kıyas modeli (gradient boosting) de eğitilir.

Hedefler SICAK motor içindir: C++ karşı-olgusal çözümleri sıcak motorla yapar.
Soğuk motorun fazladan yakıtı modelin değil simülasyondaki ısınma modelinin işidir.

Kullanım:
  python model/egit.py [--egitim build/veri/egitim.csv] [--test build/veri/test.csv]
                       [--cikti model/tuketim_modeli.json] [--grafik build/model]
                       [--kontrol] [--referans model/tuketim_modeli.json]
--kontrol : test doğruluğu eşiklerin altındaysa 1 ile çık (CTest için).
--referans: depodaki (C++'a aktarılmış) model, yeni eğitilen modelle aynı test
            verisinde karşılaştırılır; toplam tahminler %1'den fazla ayrışıyorsa
            1 ile çık. Böylece fizik değişip katsayılar eskirse test bunu yakalar.
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.linear_model import LinearRegression
from sklearn.metrics import mean_absolute_error, r2_score

# Windows konsolu/borusu Türkçe karakterlerde hata vermesin
sys.stdout.reconfigure(encoding="utf-8")

# Özellik adları C++ tarafında aynen yeniden hesaplanacak (9. adım).
# Her ad, StrategyInput alanlarından elde edilen bir ifadeye karşılık gelir.
OZELLIK_TANIMLARI = {
    "P": lambda d: d["wheel_power_W"],                          # tekerlek gücü [W]
    "T": lambda d: d["wheel_torque_Nm"],                        # tekerlek torku [Nm]
    "w": lambda d: d["wheel_speed_radps"],                      # tekerlek açısal hızı [rad/s]
    "T^2": lambda d: d["wheel_torque_Nm"] ** 2,
    "w^2": lambda d: d["wheel_speed_radps"] ** 2,
    "w^3": lambda d: d["wheel_speed_radps"] ** 3,
    "T*w": lambda d: d["wheel_torque_Nm"] * d["wheel_speed_radps"],
}

def adim_suresi(d: pd.DataFrame) -> np.ndarray:
    """Her örneğin adım süresi [s] (eski CSV'lerde sütun yoksa 1 s)."""
    if "dt_s" not in d.columns:
        return np.ones(len(d))
    return d["dt_s"].to_numpy(dtype=float)


# Model tanımları: hedef sütun (ya da hesap), özellikler, hangi satırlar kullanılır
MODELLER = {
    "ev_batarya_gucu_W": {
        "aciklama": "EV secilseydi bataryadan cekilecek motor gucu [W]",
        "ozellikler": ["P", "T^2", "w", "w^3"],
        "hedef": lambda d: d["ev_battery_power_W"],
        "filtre": lambda d: d["ev_feasible"] == 1,
    },
    "ice_yakit_debisi_kgps": {
        "aciklama": "ICE secilseydi (sarjsiz) yakit debisi [kg/s]",
        # T*w eklenmez: tanım gereği P = T*w, aynı özellik iki kez olurdu
        # (tam doğrusal bağımlılık -> katsayılar keyfi bölüşülür).
        "ozellikler": ["P", "w", "w^2", "T", "T^2"],
        "hedef": lambda d: d["ice_fuel_rate_kgps"],
        "filtre": lambda d: d["ice_feasible"] == 1,
    },
    "sarj_marjinal_yakit_kg_per_J": {
        "aciklama": "ICE modunda bataryaya verilen her joule icin ek yakit [kg/J]",
        "ozellikler": ["P", "w", "T"],
        "hedef": lambda d: (d["ice_charge_fuel_rate_kgps"] - d["ice_fuel_rate_kgps"])
        / d["ice_charge_power_W"],
        # Çok küçük şarj güçlerinde oran gürültülü; 500 W altını alma
        "filtre": lambda d: (d["ice_feasible"] == 1) & (d["ice_charge_power_W"] > 500.0),
    },
}

# --kontrol eşikleri: test kümesinin tamamında. Amaç, ileride model/veri
# değişince doğruluğun fark edilmeden bozulmasını yakalamak.
ESIKLER = {
    "ev_batarya_gucu_W": {"r2_min": 0.99, "toplam_hata_max": 0.02},
    "ice_yakit_debisi_kgps": {"r2_min": 0.95, "toplam_hata_max": 0.05},
    # Şarj maliyeti modeli tek tek örneklerde zayıf (R2 ~0.3): maliyet vites
    # değişimine bağlı sıçramalar içeriyor, lineer model bunları yakalayamıyor.
    # Strateji için önemli olan ortalama maliyet doğru olduğundan toplam hata sıkı tutulur.
    "sarj_marjinal_yakit_kg_per_J": {"r2_min": 0.15, "toplam_hata_max": 0.05},
}


def ozellik_matrisi(d: pd.DataFrame, adlar: list[str]) -> np.ndarray:
    return np.column_stack([OZELLIK_TANIMLARI[a](d).to_numpy(dtype=float) for a in adlar])


def metrikler(gercek: np.ndarray, tahmin: np.ndarray, dt: np.ndarray) -> dict:
    """R^2, ortalama mutlak hata ve toplam (çevrim boyu birikimli) bağıl hata."""
    # Toplam tüketim = sum(debi * dt); adım süresi her örnekte ayrı tutulur
    toplam = float(np.sum(gercek * dt))
    return {
        "n": int(len(gercek)),
        "r2": float(r2_score(gercek, tahmin)),
        "mae": float(mean_absolute_error(gercek, tahmin)),
        "toplam_hata": float((np.sum(tahmin * dt) - toplam) / toplam) if toplam != 0 else 0.0,
    }


def egit_ve_degerlendir(ad, tanim, egitim, test, grafik_klasoru):
    tr = egitim[tanim["filtre"](egitim)]
    te = test[tanim["filtre"](test)]
    x_tr, y_tr = ozellik_matrisi(tr, tanim["ozellikler"]), tanim["hedef"](tr).to_numpy()
    x_te, y_te = ozellik_matrisi(te, tanim["ozellikler"]), tanim["hedef"](te).to_numpy()

    lineer = LinearRegression().fit(x_tr, y_tr)
    # Kıyas modeli: yalnızca ham fiziksel büyüklükler, doğrusal olmayan ilişkiyi kendisi öğrenir
    ham = ["P", "T", "w"]
    kiyas = HistGradientBoostingRegressor(random_state=0).fit(ozellik_matrisi(tr, ham), y_tr)

    sonuc = {
        "aciklama": tanim["aciklama"],
        "ozellikler": tanim["ozellikler"],
        "katsayilar": [float(c) for c in lineer.coef_],
        "kesisim": float(lineer.intercept_),
        "egitim": metrikler(y_tr, lineer.predict(x_tr), adim_suresi(tr)),
        "test": metrikler(y_te, lineer.predict(x_te), adim_suresi(te)),
        "test_kiyas_gbr": metrikler(y_te, kiyas.predict(ozellik_matrisi(te, ham)), adim_suresi(te)),
        "test_cevrim": {},
    }
    for cevrim, grup in te.groupby("cycle", sort=False):
        sonuc["test_cevrim"][cevrim] = metrikler(
            tanim["hedef"](grup).to_numpy(),
            lineer.predict(ozellik_matrisi(grup, tanim["ozellikler"])),
            adim_suresi(grup),
        )

    if grafik_klasoru is not None:
        ciz_tahmin_gercek(ad, y_te, lineer.predict(x_te), te["cycle"].to_numpy(), grafik_klasoru)
    return sonuc


def ciz_tahmin_gercek(ad, gercek, tahmin, cevrimler, klasor: Path):
    import matplotlib

    matplotlib.use("Agg")  # ekran gerektirmeyen arka uç
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(6, 6))
    for c in dict.fromkeys(cevrimler):
        m = cevrimler == c
        ax.scatter(gercek[m], tahmin[m], s=4, alpha=0.5, label=c)
    sinir = [min(gercek.min(), tahmin.min()), max(gercek.max(), tahmin.max())]
    ax.plot(sinir, sinir, "k--", linewidth=1, label="ideal")
    ax.set_xlabel("gerçek")
    ax.set_ylabel("tahmin (lineer model)")
    ax.set_title(f"{ad} — test çevrimleri")
    ax.legend(markerscale=3, fontsize=8)
    fig.tight_layout()
    fig.savefig(klasor / f"{ad}.png", dpi=120)
    plt.close(fig)


def rapor_yaz(sonuclar: dict):
    for ad, s in sonuclar.items():
        print(f"\n=== {ad} ===  ({s['aciklama']})")
        terimler = " + ".join(f"{c:.6g}*{o}" for c, o in zip(s["katsayilar"], s["ozellikler"]))
        print(f"  model: {s['kesisim']:.6g} + {terimler}")
        print(f"  {'kume':<22}{'n':>6}{'R2':>9}{'MAE':>12}{'toplam hata':>13}")
        satirlar = [("egitim", s["egitim"]), ("test (lineer)", s["test"]),
                    ("test (kiyas GBR)", s["test_kiyas_gbr"])]
        satirlar += [(f"  {c}", m) for c, m in s["test_cevrim"].items()]
        for etiket, m in satirlar:
            print(f"  {etiket:<22}{m['n']:>6}{m['r2']:>9.4f}{m['mae']:>12.4g}"
                  f"{100 * m['toplam_hata']:>12.2f}%")


def kontrol_et(sonuclar: dict) -> bool:
    gecti = True
    for ad, esik in ESIKLER.items():
        t = sonuclar[ad]["test"]
        if t["r2"] < esik["r2_min"] or abs(t["toplam_hata"]) > esik["toplam_hata_max"]:
            print(f"KONTROL BASARISIZ: {ad}: R2={t['r2']:.4f} (min {esik['r2_min']}), "
                  f"toplam hata={100 * t['toplam_hata']:.2f}% "
                  f"(max {100 * esik['toplam_hata_max']:.0f}%)")
            gecti = False
    return gecti


def tahmin_et(model: dict, d: pd.DataFrame) -> np.ndarray:
    """JSON'daki bir lineer modelin tahmini (C++'taki LinearModel::predict ile aynı)."""
    x = ozellik_matrisi(d, model["ozellikler"])
    return model["kesisim"] + x @ np.asarray(model["katsayilar"], dtype=float)


def referansla_karsilastir(sonuclar: dict, referans: dict, test: pd.DataFrame,
                           tolerans: float = 0.01) -> bool:
    """Depodaki model ile yeni eğitilen model aynı test verisinde aynı toplamı vermeli."""
    gecti = True
    print("\nREFERANS KARSILASTIRMASI (depodaki model vs yeni egitilen, test kumesi)")
    for ad, tanim in MODELLER.items():
        if ad not in referans.get("modeller", {}):
            print(f"  {ad}: referansta yok")
            gecti = False
            continue
        te = test[tanim["filtre"](test)]
        dt = adim_suresi(te)
        yeni = float(np.sum(tahmin_et(sonuclar[ad], te) * dt))
        eski = float(np.sum(tahmin_et(referans["modeller"][ad], te) * dt))
        fark = (eski - yeni) / yeni if yeni != 0 else 0.0
        durum = "tamam" if abs(fark) <= tolerans else "ESKIMIS"
        print(f"  {ad:<30} fark = {100 * fark:+.2f}%  ({durum})")
        if abs(fark) > tolerans:
            gecti = False
    if not gecti:
        print("Depodaki katsayilar guncel fizikle uyusmuyor. Yeniden uretmek icin:\n"
              "  hea_cli --cevrim egitim --egitim-verisi build/veri/egitim.csv\n"
              "  hea_cli --cevrim test   --egitim-verisi build/veri/test.csv\n"
              "  python model/egit.py && python model/cpp_aktar.py")
    return gecti


def main() -> int:
    kok = Path(__file__).resolve().parent.parent
    p = argparse.ArgumentParser(description="Tuketim tahmin modeli egitimi")
    p.add_argument("--egitim", type=Path, default=kok / "build/veri/egitim.csv")
    p.add_argument("--test", type=Path, default=kok / "build/veri/test.csv")
    p.add_argument("--cikti", type=Path, default=kok / "model/tuketim_modeli.json")
    p.add_argument("--grafik", type=Path, default=kok / "build/model")
    p.add_argument("--grafiksiz", action="store_true", help="grafik cizme")
    p.add_argument("--kontrol", action="store_true", help="esik altinda 1 ile cik")
    p.add_argument("--referans", type=Path, default=None,
                   help="depodaki model JSON'u; yeni modelden %%1'den fazla ayrisirsa 1 ile cik")
    a = p.parse_args()

    # Referans, çıktı aynı dosyaya yazılmadan önce okunmalı
    referans = None
    if a.referans is not None:
        referans = json.loads(a.referans.read_text(encoding="utf-8"))

    egitim = pd.read_csv(a.egitim)
    test = pd.read_csv(a.test)
    # Eğitim ve test çevrimleri ayrık olmalı (model testi hiç görmemeli)
    ortak = set(egitim["cycle"]) & set(test["cycle"])
    if ortak:
        print(f"Hata: egitim ve test kumeleri ortak cevrim iceriyor: {sorted(ortak)}")
        return 1
    print(f"Egitim: {len(egitim)} ornek, {egitim['cycle'].nunique()} cevrim | "
          f"Test: {len(test)} ornek, {test['cycle'].nunique()} cevrim")

    grafik = None if a.grafiksiz else a.grafik
    if grafik is not None:
        grafik.mkdir(parents=True, exist_ok=True)

    sonuclar = {ad: egit_ve_degerlendir(ad, t, egitim, test, grafik)
                for ad, t in MODELLER.items()}
    rapor_yaz(sonuclar)

    a.cikti.parent.mkdir(parents=True, exist_ok=True)
    cikti = {
        "aciklama": "Tuketim tahmin modeli katsayilari (model/egit.py ile uretildi). "
                    "Tahmin = kesisim + sum(katsayi_i * ozellik_i). Ozellikler: "
                    "P = tekerlek gucu [W], T = tekerlek torku [Nm], "
                    "w = tekerlek acisal hizi [rad/s].",
        "egitim_cevrimleri": sorted(egitim["cycle"].unique().tolist()),
        "test_cevrimleri": sorted(test["cycle"].unique().tolist()),
        "modeller": sonuclar,
    }
    a.cikti.write_text(json.dumps(cikti, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"\nKatsayilar yazildi: {a.cikti}")
    if grafik is not None:
        print(f"Grafikler: {grafik}")

    sonuc = 0
    if a.kontrol and not kontrol_et(sonuclar):
        sonuc = 1
    if referans is not None and not referansla_karsilastir(sonuclar, referans, test):
        sonuc = 1
    return sonuc


if __name__ == "__main__":
    sys.exit(main())
