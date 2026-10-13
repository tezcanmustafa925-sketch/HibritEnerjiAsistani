#pragma once

#include "hea/mode.h"

namespace hea {

// Simülasyonun her adımda döndürdüğü sonuç. Tüm alanlar SI birimindedir;
// km/h, rpm, L/100 km gibi dönüşümleri yalnızca uygulama katmanı (cli/gui) yapar.
// Bir adım [t - dt, t] aralığını kapsar; anlık değerler aralık ortalamasıdır,
// "Used" ile biten alanlar çevrim başından beri birikimlidir.
struct StepResult {
    // --- Zaman ---
    double time_s = 0.0;   // Adımın sonundaki zaman [s]
    double dt_s = 0.0;     // Adım süresi [s]

    // --- Araç hareketi ve yol ---
    double targetSpeed_mps = 0.0;   // Çevrimin istediği hız (adım sonu) [m/s]
    double speed_mps = 0.0;         // Gerçekleşen araç hızı (adım sonu) [m/s]
    double acceleration_mps2 = 0.0; // Gerçekleşen ivme [m/s^2]
    double distance_m = 0.0;        // Çevrim başından beri katedilen yol [m]
    double grade_rad = 0.0;         // Yol eğim açısı [rad]
    double payload_kg = 0.0;        // Ek yük [kg]

    // --- Sürücü ---
    double throttle = 0.0;  // Gaz pedalı konumu [0..1]
    double brake = 0.0;     // Fren pedalı konumu [0..1]

    // --- Yol yükü kuvvetleri [N] ---
    double aeroForce_N = 0.0;     // Aerodinamik direnç
    double rollingForce_N = 0.0;  // Yuvarlanma direnci
    double gradeForce_N = 0.0;    // Eğim kuvveti (yokuş yukarı pozitif)
    double inertiaForce_N = 0.0;  // İvmelenme için gereken kuvvet
    double tractionForce_N = 0.0; // Tekerlekte istenen toplam kuvvet

    // --- Tekerlek ---
    double wheelTorque_Nm = 0.0; // İstenen tekerlek torku [Nm]
    double wheelPower_W = 0.0;   // İstenen tekerlek gücü [W]; negatif = frenleme
    double unmetWheelTorque_Nm = 0.0; // Karşılanamayan tork [Nm] (araç geride kaldı)

    // --- Güç aktarma ---
    Mode requestedMode = Mode::Electric; // Stratejinin önerisi
    Mode mode = Mode::Stopped;      // Bu adımda gerçekte uygulanan mod
    bool modeOverridden = false;    // Öneri uygulanamadı mı? (ör. EV gücü yetmedi)
    int gear = 0;                   // Vites (ICE kapalıysa 0)
    bool engineStarted = false;     // Motor bu adımda çalıştırıldı mı?
    double engineSpeed_radps = 0.0; // İçten yanmalı motor devri [rad/s]
    double engineTorque_Nm = 0.0;   // İçten yanmalı motor torku [Nm]
    double motorSpeed_radps = 0.0;  // Elektrik motoru devri [rad/s]
    double motorTorque_Nm = 0.0;    // Elektrik motoru torku [Nm]; negatif = jeneratör
    double motorElectricalPower_W = 0.0; // Motorun elektrik gücü [W]; negatif = şarj
    double frictionBrakePower_W = 0.0; // Sürtünmeli frende ısıya giden güç [W] (>= 0)

    // --- Batarya ---
    double batteryPower_W = 0.0;    // Uç gücü [W]; pozitif = deşarj (motor + yardımcı yük)
    double batteryVoltage_V = 0.0;  // Uç gerilimi [V]
    double batteryCurrent_A = 0.0;  // Akım [A]; pozitif = deşarj, negatif = şarj
    double batteryTemp_C = 0.0;     // Sıcaklık [°C] (SI istisnası, kullanıcı onaylı)
    double soc = 0.0;               // Şarj durumu [0..1] (adım sonu)

    // --- Tüketim ---
    double fuelRate_kgps = 0.0;        // Anlık yakıt debisi [kg/s]
    double fuelUsed_kg = 0.0;          // Çevrim başından beri toplam yakıt [kg]
    double batteryEnergyUsed_J = 0.0;  // Çevrim başından beri bataryadan net çekilen uç enerjisi [J]
};

} // namespace hea
