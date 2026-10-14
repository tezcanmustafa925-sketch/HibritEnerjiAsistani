#pragma once

// OTOMATIK URETILDI - elle duzenlemeyin.
// Kaynak: model/tuketim_modeli.json (model/egit.py ciktisi)
// Yeniden uretmek icin: python model/cpp_aktar.py
//
// Her model: tahmin = kesisim + sum(katsayi_i * ozellik_i)
// Ozellik adlari: P = tekerlek gucu [W], T = tekerlek torku [Nm],
// w = tekerlek acisal hizi [rad/s]; T^2, w^2, w^3, T*w bunlarin kuvvet/carpimlari.

#include <cstddef>

namespace hea {
namespace generated {

struct LinearModelCoeffs {
    const char* name;
    const char* const* features;
    const double* coefficients;
    std::size_t count;
    double intercept;
};

// EV secilseydi bataryadan cekilecek motor gucu [W]
// Test: R2 = 1.0000, toplam hata = -0.00%
inline const char* const kEvBatteryPowerFeatures[] = {"P", "T^2", "w", "w^3"};
inline const double kEvBatteryPowerCoefficients[] = {1.030927835104975, 0.0024909660944154193, 7.999999971015081, 5.120000278517785e-05};
inline const LinearModelCoeffs kEvBatteryPower = {
    "ev_batarya_gucu_W", kEvBatteryPowerFeatures, kEvBatteryPowerCoefficients, 4, 200.00000053964868};

// ICE secilseydi (sarjsiz) yakit debisi [kg/s]
// Test: R2 = 0.9980, toplam hata = +0.02%
inline const char* const kIceFuelRateFeatures[] = {"P", "w", "w^2", "T", "T^2"};
inline const double kIceFuelRateCoefficients[] = {5.5777238815730044e-08, -1.0855951712742987e-07, 2.3159646276547866e-08, -3.3133292364598526e-08, 2.306977228767735e-10};
inline const LinearModelCoeffs kIceFuelRate = {
    "ice_yakit_debisi_kgps", kIceFuelRateFeatures, kIceFuelRateCoefficients, 5, 7.527998532044506e-05};

// ICE modunda bataryaya verilen her joule icin ek yakit [kg/J]
// Test: R2 = 0.3121, toplam hata = +0.51%
inline const char* const kChargeFuelPerJouleFeatures[] = {"P", "w", "T"};
inline const double kChargeFuelPerJouleCoefficients[] = {-3.006499631043908e-13, 1.2855543791859934e-10, 3.323098868405146e-11};
inline const LinearModelCoeffs kChargeFuelPerJoule = {
    "sarj_marjinal_yakit_kg_per_J", kChargeFuelPerJouleFeatures, kChargeFuelPerJouleCoefficients, 3, 5.919695865808809e-08};

} // namespace generated
} // namespace hea
