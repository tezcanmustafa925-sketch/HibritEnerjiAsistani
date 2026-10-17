#pragma once

#include <vector>

#include "hea/units.h"

namespace hea {

// Termostatın açıldığı, motorun "sıcak" sayıldığı sıcaklık [°C]
constexpr double kEngineWarmTemp_C = 90.0;

// İçten yanmalı motor parametreleri. Varsayılanlar 1.8 L Atkinson çevrimli
// bir hibrit benzinli motora yakın (~72 kW, ~142 Nm).
struct EngineParams {
    // Rölanti / en düşük çalışma devri [rad/s]
    double idleSpeed_radps = units::rpmToRadps(1000.0);
    // Azami devir [rad/s]
    double maxSpeed_radps = units::rpmToRadps(5500.0);

    // Tam yük tork eğrisi: devir [rad/s] -> en büyük tork [Nm].
    // Aradaki devirler için doğrusal ara değer alınır.
    std::vector<double> fullLoadSpeeds_radps = {
        units::rpmToRadps(1000.0), units::rpmToRadps(2000.0), units::rpmToRadps(3000.0),
        units::rpmToRadps(4000.0), units::rpmToRadps(5000.0), units::rpmToRadps(5500.0)};
    std::vector<double> fullLoadTorques_Nm = {
        110.0, 130.0, 140.0, 142.0, 138.0, 128.0};

    // Willans hattı yakıt modeli (bkz. Engine::fuelPower):
    double indicatedEfficiency = 0.44;   // e: indike (iç) verim [-]; tepe efektif verim ~%38
    double frictionTorque0_Nm = 12.0;    // Sürtünme torku sabit kısmı [Nm]
    double frictionTorqueSlope = 0.02;   // Sürtünme torkunun devirle artışı [Nm/(rad/s)]

    // Soğuk motor (ısınma) modeli. Soğukta yağ koyu olduğu için iç sürtünme artar:
    //   T_sur(omega, T_m) = (a + b*omega) * (1 + k_soguk * x),
    //   x = clamp((T_sicak - T_m) / (T_sicak - 20), 0, 1.5)
    // 20 °C'de çalıştırılan motorda sürtünme %(100*k_soguk) fazladır, motor ısındıkça
    // sıfıra iner. Değer benim seçimim; tipik soğuk çalıştırma cezası (ilk birkaç
    // dakikada %10-20 fazla yakıt) mertebesinde sonuç verecek şekilde seçildi.
    double coldFrictionIncrease = 0.6;
    // Toplu ısıl model: C * dT/dt = f * P_yakit - hA * (T - T_ortam), T <= T_sicak
    // (termostat). C: blok + soğutma sıvısı ısı kapasitesi, f: yakıt enerjisinin
    // soğutma sıvısına giden payı, hA: ortama ısı kaybı katsayısı.
    double thermalCapacity_JperK = 50000.0;
    double heatToCoolantFraction = 0.30;
    double heatLoss_WperK = 20.0;

    bool isValid() const;
};

// İçten yanmalı motor modeli: tam yük sınırı ve yakıt tüketimi.
//
// Yakıt tüketimi Willans hattı ile modellenir: yakıttan gelen güç, motorun
// verdiği efektif güç ile iç sürtünme/pompalama kayıplarının toplamının
// indike verime bölümüdür:
//
//   P_yakit = (T*omega + T_sur(omega)*omega) / e,   T_sur(omega) = a + b*omega
//   m_dot   = P_yakit / LHV                         [kg/s]
//
// Bu model gerçekçi bir BSFC haritası üretir: düşük yükte sürtünme payı büyük
// olduğu için verim düşer (BSFC yüksek), yüksek yükte verim e'ye yaklaşır.
//   BSFC [g/kWh] = 3.6e9 / (verim * LHV)
//
// Soğuk motorda sürtünme terimi büyür (bkz. EngineParams::coldFrictionIncrease);
// motor sıcaklığını simülasyon tutar ve her çağrıya verir (varsayılan: sıcak).
//
// Varsayımlar: motor kapalıyken yakıt sıfır (start-stop), motor freni (negatif
// tork) ve yakıt kesmede motorun sürüklenme direnci modellenmiyor.
class Engine {
public:
    explicit Engine(const EngineParams& params);

    const EngineParams& params() const { return params_; }

    // Verilen devirde tam yük torku [Nm]; çalışma aralığı dışında 0.
    double maxTorque(double speed_radps) const;

    // Soğukluk çarpanı 1 + k_soguk * x [-] (sıcak motorda tam 1)
    double coldFrictionFactor(double engineTemp_C) const;

    // İç sürtünme torku T_sur = (a + b*omega) * soğukluk çarpanı [Nm]
    double frictionTorque(double speed_radps, double engineTemp_C = kEngineWarmTemp_C) const;

    // Yakıttan alınan kimyasal güç [W] (T >= 0 kabul edilir)
    double fuelPower(double torque_Nm, double speed_radps,
                     double engineTemp_C = kEngineWarmTemp_C) const;

    // Yakıt debisi [kg/s]
    double fuelRate(double torque_Nm, double speed_radps,
                    double engineTemp_C = kEngineWarmTemp_C) const;

    // Efektif verim = T*omega / P_yakit [-]
    double efficiency(double torque_Nm, double speed_radps,
                      double engineTemp_C = kEngineWarmTemp_C) const;

    // Isıl modelin bir adımı: dt sonundaki motor sıcaklığı [°C]
    double nextTemperature(double engineTemp_C, double fuelPower_W, double ambientTemp_C,
                           double dt_s) const;

    // Devir çalışma aralığında ve tork tam yük sınırı içinde mi?
    bool canDeliver(double torque_Nm, double speed_radps) const;

private:
    EngineParams params_;
};

} // namespace hea
