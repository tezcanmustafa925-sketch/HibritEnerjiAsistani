#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hea/drive_input.h"

namespace hea {

// Sürüş çevrimi: sabit örnekleme aralığında DriveInput dizisi.
// samples[i].time_s = i * dt_s. İlk örnek t = 0'dadır.
struct DriveCycle {
    std::string name;
    double dt_s = 1.0;
    std::vector<DriveInput> samples;

    double duration_s() const;
    // Katedilen yol [m]: hız örneklerinin yamuk kuralıyla integrali
    double distance_m() const;
    double maxSpeed_mps() const;
    // Ortalama hız [m/s] = yol / süre (duruşlar dahil)
    double averageSpeed_mps() const;
    // Aracın durduğu (hedef hız ~0) sürenin oranı [0..1]
    double stopFraction() const;
    // Ardışık örnekler arasındaki en büyük ivme ve yavaşlama [m/s^2]
    double maxAcceleration_mps2() const;
    double maxDeceleration_mps2() const;
};

// Yol eğim profilinin bir noktası: verilen mesafede eğim. Noktalar arasında
// eğim doğrusal değişir (yol aniden kırılmaz); son noktadan sonra sabit kalır.
struct GradePoint {
    double distance_m = 0.0;
    double gradePercent = 0.0; // Yüzde eğim; pozitif = yokuş yukarı
};

// Verilen mesafedeki eğim açısı [rad]. Profil boşsa düz yol (0).
double gradeAtDistance(const std::vector<GradePoint>& profile, double distance_m);

// Parça parça çevrim oluşturucu. Hız profili zaman, eğim profili mesafe tabanlıdır.
// Örnek:
//   DriveCycle c = CycleBuilder("ornek").hold(10).rampTo(kmhToMps(50), 1.0)
//                      .hold(30).rampTo(0, 1.5).hold(5).build();
class CycleBuilder {
public:
    explicit CycleBuilder(std::string name, double dt_s = 1.0);

    // Mevcut hızı verilen süre boyunca koru (hız 0 ise araç bekler).
    CycleBuilder& hold(double duration_s);
    // Hızı sabit ivmeyle hedefe götür. rate_mps2 > 0 ivmenin büyüklüğüdür;
    // yükselme ve iniş için aynı fonksiyon kullanılır. Adım sayısı yukarı
    // yuvarlandığı için gerçek ivme rate_mps2'yi aşmaz.
    CycleBuilder& rampTo(double targetSpeed_mps, double rate_mps2);

    CycleBuilder& grade(std::vector<GradePoint> profile);
    CycleBuilder& payload(double kg);
    CycleBuilder& ambient(double temp_C);

    double currentSpeed() const { return speeds_.back(); }
    double currentTime() const { return static_cast<double>(speeds_.size() - 1) * dt_; }

    DriveCycle build() const;

private:
    std::string name_;
    double dt_;
    std::vector<double> speeds_;
    std::vector<GradePoint> grade_;
    double payload_kg_ = 75.0;
    double ambient_C_ = 25.0;
};

// ----------------------------------------------------------------------------
// Hazır çevrim üreteçleri. Aynı tohum (seed) her zaman aynı çevrimi üretir
// (platformdan bağımsız, kendi rastgele sayı dönüşümümüz kullanılır).
//
//   Şehir   : sık dur-kalk, 20-55 km/h, hafif eğim (±%2), ~20 dk
//   Otoyol  : 90-130 km/h seyir ve hız değişimleri, eğim ±%3, ~20 dk
//   Karma   : şehir + çevre yolu + şehir, tepelik (±%5), ~30 dk
// ----------------------------------------------------------------------------
DriveCycle makeUrbanCycle(std::uint32_t seed);
DriveCycle makeHighwayCycle(std::uint32_t seed);
DriveCycle makeMixedCycle(std::uint32_t seed);

// Eğitim ve test kümeleri. Aynı türlerden ama FARKLI tohumlarla üretilir, böylece
// model test çevrimlerini eğitimde hiç görmez. Test kümesinde ayrıca eğitimde
// olmayan bir koşul (ağır yük + dik tepeler + soğuk hava) bulunur.
std::vector<DriveCycle> trainingCycles();
std::vector<DriveCycle> testCycles();

} // namespace hea
