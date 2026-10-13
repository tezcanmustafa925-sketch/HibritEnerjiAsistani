#pragma once

#include "hea/vehicle_params.h"

namespace hea {

// Tekerlekte aracı hareket ettirmek için yenilmesi gereken kuvvetler [N].
// İşaret kuralı: pozitif = araç bu kuvveti tekerlekten sağlamalı (tahrik),
// negatif = kuvvet aracı kendiliğinden itiyor / frenlenmesi gerekiyor.
struct RoadLoad {
    double aero_N = 0.0;     // Aerodinamik direnç
    double rolling_N = 0.0;  // Yuvarlanma direnci
    double grade_N = 0.0;    // Eğim (yokuş yukarı pozitif, iniş negatif)
    double inertia_N = 0.0;  // İvmelenme (hızlanma pozitif, yavaşlama negatif)
    double total_N = 0.0;    // Toplam çekiş kuvveti = yukarıdakilerin toplamı
};

// Boylamasına (tek boyutlu) araç dinamiği modeli.
//
// Tekerlekte gereken toplam çekiş kuvveti, Newton'un 2. yasasından:
//
//   F_cekis = F_aero + F_yuvarlanma + F_egim + F_atalet
//
// Varsayımlar:
//   - Araç düz bir doğru üzerinde ilerler, yanal dinamik ve viraj yok.
//   - Rüzgâr yok (havaya göre hız = araç hızı).
//   - Hava yoğunluğu sabit (deniz seviyesi, 15 °C).
//   - Lastik kayması yok: v = omega_tekerlek * r.
//   - Araç geri gitmez (hız >= 0).
class VehicleModel {
public:
    explicit VehicleModel(const VehicleParams& params);

    const VehicleParams& params() const { return params_; }

    // Toplam kütle m = boş kütle + yük [kg]
    double totalMass(double payload_kg) const;

    // Aerodinamik direnç [N]:
    //   F_aero = 1/2 * rho * Cd * A * v^2
    //   rho: hava yoğunluğu, Cd: direnç katsayısı, A: ön kesit alanı, v: hız.
    //   Dinamik basınç (1/2 rho v^2) ile etkin alanın (Cd*A) çarpımıdır;
    //   hızın karesiyle arttığı için otoyolda baskın direnç budur.
    double aeroForce(double speed_mps) const;

    // Yuvarlanma direnci [N]:
    //   F_yuv = Crr * m * g * cos(theta)
    //   Lastiğin yol temasında ezilip açılmasından kaynaklanan kayıp.
    //   m*g*cos(theta) yola dik normal kuvvettir; eğimde biraz azalır.
    //   Hızdan bağımsız kabul edilir. Araç dururken (v = 0) sıfır alınır,
    //   çünkü duran araçta bu kuvvet harekete karşı koymaz.
    double rollingForce(double speed_mps, double mass_kg, double grade_rad) const;

    // Eğim kuvveti [N]:
    //   F_egim = m * g * sin(theta)
    //   Ağırlığın yola paralel bileşeni. Yokuş yukarı (theta > 0) pozitif,
    //   yokuş aşağı negatiftir (aracı iter, rejeneratif frenleme fırsatı).
    double gradeForce(double mass_kg, double grade_rad) const;

    // Atalet (ivme) kuvveti [N]:
    //   F_atalet = delta * m * a
    //   a: boylamasına ivme. delta (dönen kütle katsayısı) tekerlek, aks ve
    //   motor rotoru gibi dönen parçaların açısal hızlanması için gereken ek
    //   kuvveti temsil eder. Basitleştirme: delta toplam kütleye uygulanır.
    double inertiaForce(double mass_kg, double accel_mps2) const;

    // Tüm yol yükü kuvvetleri ve toplamı.
    RoadLoad roadLoad(double speed_mps, double accel_mps2,
                      double grade_rad, double payload_kg) const;

    // Tekerlek torku [Nm]: T = F * r (r: dinamik tekerlek yarıçapı)
    double wheelTorque(double force_N) const;

    // Tekerlek gücü [W]: P = F * v. Negatif güç = frenleme (geri kazanılabilir enerji).
    static double wheelPower(double force_N, double speed_mps);

    // Tekerlek açısal hızı [rad/s]: omega = v / r (kayma yok varsayımı)
    double wheelSpeed(double speed_mps) const;

private:
    VehicleParams params_;
};

} // namespace hea
