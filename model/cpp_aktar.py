"""Model katsayılarını C++ başlık dosyasına aktarır.

Girdi : model/tuketim_modeli.json            (egit.py çıktısı)
Çıktı : core/include/hea/consumption_model_coeffs.h

Katsayılar elle kopyalanmaz; böylece yazım hatası olmaz ve model yeniden
eğitildiğinde C++ tarafı tek komutla güncellenir.

Kullanım:
  python model/cpp_aktar.py            # başlık dosyasını yaz
  python model/cpp_aktar.py --kontrol  # dosya JSON ile güncel değilse 1 ile çık
"""

import argparse
import json
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

# JSON'daki model adı -> C++ değişken adı öneki
MODEL_ADLARI = {
    "ev_batarya_gucu_W": "kEvBatteryPower",
    "ice_yakit_debisi_kgps": "kIceFuelRate",
    "sarj_marjinal_yakit_kg_per_J": "kChargeFuelPerJoule",
}


def sayi(x: float) -> str:
    # repr: double'ı kayıpsız geri okutan en kısa gösterim
    return repr(float(x))


def baslik_uret(veri: dict, kaynak: str) -> str:
    satirlar = [
        "#pragma once",
        "",
        "// OTOMATIK URETILDI - elle duzenlemeyin.",
        f"// Kaynak: {kaynak} (model/egit.py ciktisi)",
        "// Yeniden uretmek icin: python model/cpp_aktar.py",
        "//",
        "// Her model: tahmin = kesisim + sum(katsayi_i * ozellik_i)",
        "// Ozellik adlari: P = tekerlek gucu [W], T = tekerlek torku [Nm],",
        "// w = tekerlek acisal hizi [rad/s]; T^2, w^2, w^3, T*w bunlarin kuvvet/carpimlari.",
        "",
        "#include <cstddef>",
        "",
        "namespace hea {",
        "namespace generated {",
        "",
        "struct LinearModelCoeffs {",
        "    const char* name;",
        "    const char* const* features;",
        "    const double* coefficients;",
        "    std::size_t count;",
        "    double intercept;",
        "};",
        "",
    ]
    for json_ad, cpp_ad in MODEL_ADLARI.items():
        m = veri["modeller"][json_ad]
        ozellikler = ", ".join(f'"{o}"' for o in m["ozellikler"])
        katsayilar = ", ".join(sayi(k) for k in m["katsayilar"])
        test = m["test"]
        satirlar += [
            f"// {m['aciklama']}",
            f"// Test: R2 = {test['r2']:.4f}, toplam hata = {100 * test['toplam_hata']:+.2f}%",
            f"inline const char* const {cpp_ad}Features[] = {{{ozellikler}}};",
            f"inline const double {cpp_ad}Coefficients[] = {{{katsayilar}}};",
            f"inline const LinearModelCoeffs {cpp_ad} = {{",
            f'    "{json_ad}", {cpp_ad}Features, {cpp_ad}Coefficients, '
            f"{len(m['katsayilar'])}, {sayi(m['kesisim'])}}};",
            "",
        ]
    satirlar += ["} // namespace generated", "} // namespace hea", ""]
    return "\n".join(satirlar)


def main() -> int:
    kok = Path(__file__).resolve().parent.parent
    p = argparse.ArgumentParser(description="Model katsayilarini C++ basligina aktar")
    p.add_argument("--json", type=Path, default=kok / "model/tuketim_modeli.json")
    p.add_argument("--cikti", type=Path,
                   default=kok / "core/include/hea/consumption_model_coeffs.h")
    p.add_argument("--kontrol", action="store_true",
                   help="yazma; dosya guncel degilse 1 ile cik")
    a = p.parse_args()

    veri = json.loads(a.json.read_text(encoding="utf-8"))
    icerik = baslik_uret(veri, "model/tuketim_modeli.json")

    if a.kontrol:
        mevcut = a.cikti.read_text(encoding="utf-8") if a.cikti.exists() else ""
        if mevcut != icerik:
            print(f"GUNCEL DEGIL: {a.cikti} JSON ile uyusmuyor. "
                  f"'python model/cpp_aktar.py' calistirin.")
            return 1
        print(f"Guncel: {a.cikti}")
        return 0

    a.cikti.write_text(icerik, encoding="utf-8", newline="\n")
    print(f"Yazildi: {a.cikti}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
