#pragma once

#include <vector>

#include "hea/electric_motor.h"
#include "hea/engine.h"
#include "hea/mode.h"

namespace hea {

// Aktarma organı (dişli) parametreleri.
//
// Mimari (paralel hibrit, basitleştirilmiş):
//   - İçten yanmalı motor -> kavrama -> 5 ileri vites kutusu -> diferansiyel -> tekerlek
//   - Elektrik motoru -> sabit redüksiyon -> tekerlek aksı (vites kutusunun çıkışında)
// İki motor da tekerleğe mekanik olarak bağlıdır; elektrik motoru devri her zaman
// araç hızıyla orantılıdır.
struct DrivetrainParams {
    double motorGearRatio = 8.0;          // Elektrik motoru redüksiyon oranı [-]
    double motorGearEfficiency = 0.97;    // Redüksiyon dişlisi verimi [-]
    std::vector<double> gearRatios = {3.5, 2.0, 1.4, 1.0, 0.8}; // Vites oranları [-]
    double finalDriveRatio = 3.9;         // Diferansiyel oranı [-]
    double gearboxEfficiency = 0.95;      // Vites kutusu + diferansiyel verimi [-]
    // Bu tekerlek devrinin altında rejeneratif frenleme yapılmaz [rad/s]
    // (5 rad/s * 0.30 m = 1.5 m/s ~ 5.4 km/h). Çok düşük hızda motor verimi
    // düşük, geri kazanım kayıplardan az olur; araç sürtünmeli frenle durdurulur.
    double regenMinWheelSpeed_radps = 5.0;

    bool isValid() const;
};

// Güç aktarma organından bu adım için istenen.
struct PowertrainRequest {
    double wheelTorque_Nm = 0.0;     // Tekerlekte istenen tork [Nm]; negatif = frenleme
    double wheelSpeed_radps = 0.0;   // Tekerlek açısal hızı [rad/s]
    Mode requestedMode = Mode::Electric; // Stratejinin önerisi: Electric veya Engine
    // Engine modunda bataryayı şarj etmek için istenen elektrik gücü [W] (>= 0).
    // Motor bu gücü jeneratör olarak üretir, ek yükü içten yanmalı motor karşılar.
    double chargePower_W = 0.0;
    // Bataryanın bu adımdaki sınırları [W] (pozitif büyüklük), bkz. Battery::available*
    double batteryMaxDischarge_W = 0.0;
    double batteryMaxCharge_W = 0.0;
};

// Güç aktarma organının çözümü.
struct PowertrainResult {
    Mode mode = Mode::Stopped;  // Gerçekte uygulanan mod
    bool overridden = false;    // İstenen mod uygulanamadı mı? (ör. EV gücü yetmedi)

    int gear = 0;               // Kullanılan vites (1..N), motor kapalıysa 0
    bool clutchSlip = false;    // Kalkışta kavrama kaydırılıyor mu?
    double engineSpeed_radps = 0.0;
    double engineTorque_Nm = 0.0;
    double fuelRate_kgps = 0.0;

    double motorSpeed_radps = 0.0;
    double motorTorque_Nm = 0.0;           // Negatif = jeneratör
    double motorElectricalPower_W = 0.0;   // Bataryadan istenen güç; negatif = şarj

    double frictionBrakeTorque_Nm = 0.0;   // Sürtünmeli fren torku (tekerlekte, <= 0)
    double deliveredWheelTorque_Nm = 0.0;  // Gerçekte sağlanan tekerlek torku
    double unmetWheelTorque_Nm = 0.0;      // Karşılanamayan tork (>= 0); araç yavaş ivmelenir
};

// Tekerlek talebini iki motor ve frenler arasında paylaştırır.
//
// Dişli bağıntıları (G: toplam oran, eta: verim):
//   Devir:            omega_motor = omega_tekerlek * G
//   Tahrikte tork:    T_motor = T_tekerlek / (G * eta)   (kayıp motor tarafından karşılanır)
//   Geri kazanımda:   T_motor = T_tekerlek * eta / G     (kayıp tekerlek tarafından karşılanır)
//
// Karar kuralları:
//   1. Araç duruyor ve tahrik istenmiyor           -> Stopped
//   2. Tekerlek torku negatif (fren / iniş)         -> RegenBraking: önce elektrik motoru
//      jeneratör olarak (motor ve batarya şarj sınırına kadar), kalanı sürtünmeli fren
//   3. Electric istendi ve motor + batarya yetiyor  -> Electric
//      yetmiyorsa                                   -> Engine (overridden = true)
//   4. Engine: tahrik ve şarj gerekmiyorsa yakıt kesilir (DFCO, yakıt = 0).
//      Aksi halde yakıt debisi en düşük vites seçilir. Motor yetmezse elektrik motoru
//      takviye eder; o da yetmezse eksik tork unmetWheelTorque_Nm olarak bildirilir.
class Powertrain {
public:
    Powertrain(const MotorParams& motor, const EngineParams& engine,
               const DrivetrainParams& drivetrain);

    const ElectricMotor& motor() const { return motor_; }
    const Engine& engine() const { return engine_; }
    const DrivetrainParams& drivetrain() const { return drivetrain_; }

    PowertrainResult resolve(const PowertrainRequest& req) const;

private:
    PowertrainResult solveRegen(const PowertrainRequest& req) const;
    bool solveElectric(const PowertrainRequest& req, PowertrainResult& out) const;
    PowertrainResult solveEngine(const PowertrainRequest& req) const;
    // Engine modunda verilen eksen torku için en düşük yakıtlı vitesi arar.
    bool bestGear(double axleTorque_Nm, double wheelSpeed_radps, PowertrainResult& out) const;

    ElectricMotor motor_;
    Engine engine_;
    DrivetrainParams drivetrain_;
};

} // namespace hea
