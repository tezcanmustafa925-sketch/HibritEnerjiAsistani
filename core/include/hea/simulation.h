#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "hea/battery.h"
#include "hea/defaults.h"
#include "hea/drive_cycle.h"
#include "hea/powertrain.h"
#include "hea/step_result.h"
#include "hea/strategy.h"
#include "hea/vehicle_model.h"

namespace hea {

// Klima / ısıtma (HVAC) elektrik yükü. Hibritlerde klima kompresörü elektriklidir;
// ısıtma büyük ölçüde motor ısısıyla yapılır, elektrik yalnızca fan ve yardımcı
// ısıtıcı içindir. Basit, ortam sıcaklığına bağlı doğrusal model (benim seçimim):
//   T_ortam > T_konfor + bant:  P = k_sogutma * (T_ortam - T_konfor)
//   T_ortam < T_konfor - bant:  P = k_isitma  * (T_konfor - T_ortam)
//   arada (konfor bandı):       P = 0
// En fazla maxPower_W. Güneş yükü, kabin ısıl ataleti ve nem modellenmiyor.
struct HvacParams {
    bool enabled = true;
    double comfortTemp_C = 22.0;
    double deadband_K = 3.0;
    double coolingCoeff_WperK = 100.0; // 32 °C'de ~1 kW
    double heatingCoeff_WperK = 30.0;  // 0 °C'de ~0.7 kW
    double maxPower_W = 3000.0;

    bool isValid() const;
    // Verilen ortam sıcaklığında çekilen elektrik gücü [W]
    double power_W(double ambientTemp_C) const;
};

// Simülasyonun tüm parametreleri.
struct SimulationConfig {
    VehicleParams vehicle;
    BatteryParams battery;
    MotorParams motor;
    EngineParams engine;
    DrivetrainParams drivetrain;
    double initialSoc = 0.60;
    // Yüksek gerilim bataryasından sürekli çekilen yardımcı yük [W]
    // (12 V sistem, kontrol üniteleri, fan; DC/DC üzerinden). Klima hariç.
    double auxiliaryPower_W = 300.0;
    HvacParams hvac;
    // Fren pedalını normalize etmek için lastik-yol sürtünme katsayısı mu [-]:
    // en büyük fren kuvveti = mu * m * g
    double tireRoadFriction = 0.9;
    // Motorun her çalıştırılmasında harcanan ek yakıt [kg]. Marş sırasında
    // devri yükseltmek, yanmayı kararlı hale getirmek ve katalizörü sıcak tutmak
    // için harcanan yakıtı temsil eder. 0.2 g, rölantide ~2-3 saniyelik yakıta
    // denk, benim seçtiğim bir değerdir. Sık açıp kapatan stratejileri cezalandırır.
    double engineStartFuel_kg = defaults::kEngineStartFuel_kg;
    // true: motor ortam sıcaklığında (soğuk) başlar; false: sıcak (90 °C) başlar
    bool engineColdStart = true;

    // Tüm parametreler fiziksel olarak anlamlıysa boş metin, değilse hatanın açıklaması.
    std::string validate() const;
    bool isValid() const { return validate().empty(); }
};

// Bir çevrimi adım adım çalıştıran simülasyon.
//
// Yöntem: geriye dönük (backward-facing) model + ideal sürücü.
// Her adımda çevrimin hedef hızına ulaşmak için gereken ivme hesaplanır,
// araç modeli bundan tekerlek torkunu bulur, strateji modu seçer, güç aktarma
// organı torku motorlar ve frenler arasında paylaştırır, batarya güncellenir.
//
// Adım [t_{i-1}, t_i] için:
//   a_istenen = (v_hedef,i - v_{i-1}) / dt
//   v_ort     = (v_{i-1} + v_hedef,i) / 2     (kuvvetler bu hızda hesaplanır)
//   T_tekerlek = F_cekis(v_ort, a_istenen) * r,   omega_tekerlek = v_ort / r
// Güç aktarma organı torkun tamamını veremezse araç hedefin gerisinde kalır:
//   a_gercek = (F_verilen - F_aero - F_yuv - F_egim) / (delta * m)
//   v_i      = v_{i-1} + a_gercek * dt
// Araç sonraki adımlarda hedefi yakalamaya çalışır.
//
// Varsayımlar:
//   - Sürücü idealdir: gücün yettiği her durumda hedef hızı tam izler.
//     Gaz ve fren pedalı bu talepten türetilen göstergelerdir:
//       gaz  = T_tekerlek / T_tekerlek,max(omega)       [0..1]
//       fren = -F_cekis / (mu * m * g)                  [0..1]
//   - Araç dururken (v_{i-1} = v_hedef,i = 0) fren aracı tutar, tahrik yok.
//   - Yardımcı yük (sabit yük + klima) bataryadan çekilir; batarya sınırları
//     stratejiye ve güç aktarma organına bu yük düşülerek verilir. Batarya yükü
//     karşılayamıyorsa (SOC alt sınırda) eksik kısım, motor çalışıyorsa jeneratörle
//     üretilir; yine karşılanamayan güç unmetElectricalPower_W olarak raporlanır
//     (enerji sessizce kaybolmaz).
//   - Adım içinde eğim, yük ve ortam sıcaklığı sabittir (adım sonundaki örnek).
//   - Motor dönüyor sayılır: yakıt yakıyorsa ya da strateji motoru açık tutuyorken
//     araç hareket halindeyse (fren / iniş anında yakıt kesme, DFCO). Önceki adımda
//     dönmüyorken bu adımda dönüyorsa bir çalıştırma sayılır ve engineStartFuel_kg
//     eklenir. Strateji motoru kapatırsa (ör. kısa bir frenlemede) sonraki ilk
//     tahrikte yeniden çalıştırma sayılır.
//   - Motor sıcaklığı toplu ısıl modelle izlenir; soğuk motor daha çok yakar.
class Simulation {
public:
    // cycle ve strategy simülasyon boyunca yaşamalıdır (referans tutulur).
    // Yapılandırma geçersizse veya çevrimin adım süresi pozitif değilse
    // std::invalid_argument fırlatır (core ekrana yazmaz, hatayı çağırana bildirir).
    Simulation(const SimulationConfig& config, const DriveCycle& cycle, Strategy& strategy);

    bool finished() const { return next_ >= cycle_.samples.size(); }
    // Bir adım ilerle. finished() iken çağrılırsa std::logic_error fırlatır.
    StepResult step();

    const Battery& battery() const { return battery_; }
    double speed_mps() const { return speed_; }
    double engineTemp_C() const { return engineTemp_C_; }

private:
    SimulationConfig config_;
    const DriveCycle& cycle_;
    Strategy& strategy_;
    VehicleModel vehicle_;
    Powertrain powertrain_;
    Battery battery_;

    std::size_t next_ = 1;   // İşlenecek sonraki örnek
    bool engineRunning_ = false; // Önceki adımda motor dönüyor muydu?
    double engineTemp_C_ = 0.0;
    double speed_ = 0.0;     // Gerçekleşen hız [m/s]
    double distance_ = 0.0;
    double fuelUsed_ = 0.0;
    double batteryEnergy_ = 0.0;
};

// Bütün çevrimi çalıştırır; her adımın sonucunu döndürür (örnek sayısı - 1 adet).
std::vector<StepResult> runCycle(const SimulationConfig& config, const DriveCycle& cycle,
                                 Strategy& strategy);

// Bir çevrim çalıştırmasının özeti (SI). Ekrana yazmaz; uygulama katmanı biçimlendirir.
struct RunSummary {
    double duration_s = 0.0;
    double distance_m = 0.0;
    double fuel_kg = 0.0;
    double fuel_L = 0.0;               // fuel_kg / benzin yoğunluğu
    double fuelPer100km_L = 0.0;       // L/100 km (SOC düzeltmesi YOK, bkz. 10. adım)
    double socStart = 0.0;
    double socEnd = 0.0;
    double batteryEnergyUsed_J = 0.0;  // Net uç enerjisi (pozitif = bataryadan çekilen)
    double regenEnergy_J = 0.0;        // Rejeneratif frenle bataryaya giden enerji
    double frictionBrakeEnergy_J = 0.0;// Sürtünmeli frende ısıya giden enerji
    double auxiliaryEnergy_J = 0.0;    // Yardımcı yük + klima enerjisi
    double unmetElectricalEnergy_J = 0.0; // Batarya/jeneratörün karşılayamadığı elektrik enerjisi
    // Mod süre oranları [0..1]
    double timeElectric = 0.0;
    double timeEngine = 0.0;
    double timeRegen = 0.0;
    double timeStopped = 0.0;
    double overriddenFraction = 0.0;   // Stratejinin önerisinin uygulanamadığı adım oranı
    int engineStarts = 0;              // Motor çalıştırma sayısı
    double maxSpeedError_mps = 0.0;    // max |v_hedef - v| (izleme hatası)
};

RunSummary summarize(const std::vector<StepResult>& steps, double initialSoc);

} // namespace hea
