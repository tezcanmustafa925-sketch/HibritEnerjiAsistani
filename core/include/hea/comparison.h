#pragma once

#include <vector>

#include "hea/defaults.h"
#include "hea/drive_cycle.h"
#include "hea/simulation.h"
#include "hea/strategy.h"
#include "hea/units.h"

namespace hea {

// ----------------------------------------------------------------------------
// SOC düzeltmeli yakıt karşılaştırması (10. adım)
//
// Sorun: Stratejiler çevrimi farklı SOC ile bitirir. Bataryayı boşaltan strateji
// daha az yakıt harcamış görünür ama aslında yakıtı "borçlanmıştır"; dolduran
// strateji ise fazla yakıtı bataryada saklamıştır. Ham L/100 km adil değildir.
//
// Seçilen yöntem (ANA): doğrusal SOC düzeltmesi, SAE J1711 / UN R101 mantığı.
//   Aynı strateji, aynı çevrimde N farklı başlangıç SOC'si ile çalıştırılır.
//   Her koşu bir nokta verir: (E_bat, m_yakit)
//     E_bat : bataryadan çıkan net DEPOLANAN enerji [J] (pozitif = boşalttı),
//             SOC_başlangıç ve SOC_bitiş'ten hesaplanır (storedEnergyReleased_J).
//             Uç enerjisi kullanılmaz: I^2*R kayıplarını içerdiği için soğukta ve
//             yüksek akımda SOC düştüğü halde negatif çıkabilir. E_bat = 0 tam olarak
//             "aynı SOC ile bitirme" demektir (SAE J1711 de net Ah ile düzeltir).
//     m_yakit: harcanan yakıt [kg]
//   Düzeltilmiş yakıt, batarya enerjisinin net değişmediği (E_bat = 0) durumdaki
//   yakıttır. E_bat'a göre sıralanan noktalarda sıfırın iki yanındaki en yakın iki
//   nokta arasında doğrusal ara değerle bulunur:
//     m_duz = m_1 + (m_2 - m_1) * (0 - E_1) / (E_2 - E_1),   E_1 <= 0 <= E_2
//   Bu iki nokta arasındaki eğim b = (m_2 - m_1) / (E_2 - E_1) [kg/J], o stratejinin
//   batarya enerjisini yakıtla yerine koyma maliyetidir; her strateji için ayrı
//   ölçülür, dışarıdan verilmez.
//   Neden tüm noktalara tek doğru değil: kural tabanlı strateji SOC 0.70 üstünde
//   davranış değiştirir (EV eşiği yükselir), ilişki tüm aralıkta doğrusal değildir.
//   Tüm noktalara oturtulan doğrunun R^2'si yine de doğrusallık tanısı olarak tutulur.
//   Sıfır kapsanmıyorsa (tüm E_bat aynı işaretli) en yakın iki noktanın doğrusuyla
//   dış değerleme yapılır ve sonuç işaretlenir.
//
// Kontrol yöntemi: eşdeğer yakıt. m_esd = m_yakit + k * E_bat, sabit k [kg/J].
//   k = 2.8 / LHV seçilir (8c: şarj maliyeti). Bu, model tabanlı stratejinin
//   kendi s0 değeriyle aynı olduğundan o stratejinin lehine olabilir; bu yüzden
//   ana yöntem değil, yalnızca sıralamanın tutarlılığını kontrol için kullanılır.
// ----------------------------------------------------------------------------

// En küçük kareler doğrusu y = intercept + slope * x
struct LineFit {
    double intercept = 0.0;
    double slope = 0.0;
    double r2 = 0.0;  // Belirleme katsayısı (1 = noktalar tam doğru üzerinde)
};
LineFit fitLine(const std::vector<double>& x, const std::vector<double>& y);

// Düzeltme için bir koşunun özeti
struct CorrectionPoint {
    double initialSoc = 0.0;
    double finalSoc = 0.0;
    double batteryEnergy_J = 0.0; // net depolanan enerji kaybı, pozitif = boşaltıldı
    double fuel_kg = 0.0;
};

struct SocCorrectedFuel {
    std::vector<CorrectionPoint> points;
    double distance_m = 0.0;
    LineFit fit;                  // Tüm noktalara oturtulan doğru (doğrusallık tanısı)
    double correctedFuel_kg = 0.0;// E_bat = 0'daki yakıt (yerel ara/dış değer)
    double localSlope_kgPerJ = 0.0; // Kullanılan iki nokta arasındaki eğim b
    // E_bat = 0, noktaların aralığında değilse true: düzeltilmiş değer veri
    // aralığının dışından tahmin edildi (dış değerleme), daha az güvenilir.
    bool extrapolated = false;

    double fuel_kg() const { return correctedFuel_kg; }
    double fuelPer100km_L() const;
};

// Varsayılan başlangıç SOC'leri: SOC penceresi (0.30-0.80) içinde, 0.40-0.78.
// Üst uç, stratejilerin çevrimi "doğal olarak" bitirdiği SOC'nin (otoyolda ~0.70)
// üstünde olmalı ki E_bat = 0 iki yandan kapsansın.
std::vector<double> defaultCorrectionSocs();

// Stratejiyi her başlangıç SOC'si için çalıştırıp doğrusal SOC düzeltmesi yapar.
// Batarya penceresinin (socMin..socMax) dışındaki başlangıç SOC'leri atlanır.
SocCorrectedFuel socCorrectedFuel(const SimulationConfig& config, const DriveCycle& cycle,
                                  Strategy& strategy,
                                  const std::vector<double>& initialSocs = defaultCorrectionSocs());

// Kontrol yöntemi için varsayılan k [kg/J]: 1 J batarya enerjisi = 2.8 J yakıt
constexpr double kDefaultFuelPerBatteryJoule = defaults::kEquivalenceFactor / constants::kGasolineLhv;

// Eşdeğer yakıt [kg]: m_yakit + k * E_bat (E_bat: depolanan enerji kaybı)
double equivalentFuel_kg(double fuel_kg, double batteryEnergy_J,
                         double kgPerJoule = kDefaultFuelPerBatteryJoule);

// L/100 km = (m / yoğunluk) * 1e5 / mesafe[m]
double litersPer100km(double fuel_kg, double distance_m);

} // namespace hea
