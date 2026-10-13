// Elektrik motoru, içten yanmalı motor ve güç aktarma organı testleri.
// Beklenen değerler varsayılan parametrelerle elle hesaplandı.

#include <cmath>

#include "hea/electric_motor.h"
#include "hea/engine.h"
#include "hea/powertrain.h"
#include "hea/units.h"
#include "test_util.h"

using namespace hea;

static const ElectricMotor kMotor{MotorParams{}};
static const Engine kEngine{EngineParams{}};
static const Powertrain kPt{MotorParams{}, EngineParams{}, DrivetrainParams{}};

// Tekerlek yarıçapı 0.30 m: 50 km/h -> 13.8889 / 0.3 = 46.2963 rad/s
static const double kWheel50 = units::kmhToMps(50.0) / 0.30;
static const double kWheel100 = units::kmhToMps(100.0) / 0.30;

// ---------------------------------------------------------------- Elektrik motoru

static void testMotorParams()
{
    HEA_CHECK(MotorParams{}.isValid());
    HEA_CHECK(EngineParams{}.isValid());
    HEA_CHECK(DrivetrainParams{}.isValid());
}

static void testMotorTorqueLimit()
{
    // T_max = min(163, 53000 / omega)
    HEA_CHECK_NEAR(kMotor.maxTorque(0.0), 163.0, 1e-12);
    HEA_CHECK_NEAR(kMotor.maxTorque(200.0), 163.0, 1e-12);   // 53000/200 = 265 > 163
    HEA_CHECK_NEAR(kMotor.maxTorque(1000.0), 53.0, 1e-12);   // güç sınırlı bölge
    HEA_CHECK_NEAR(kMotor.maxTorque(1500.0), 0.0, 0.0);      // azami devrin üstü
    HEA_CHECK(!kMotor.canDeliver(100.0, 600.0));              // 53000/600 = 88.3 Nm
    HEA_CHECK(kMotor.canDeliver(-80.0, 600.0));               // jeneratörde de aynı sınır
}

static void testMotorLossAndPower()
{
    // T = 60 Nm, omega = 500 rad/s:
    //   kayıp = 0.15*3600 + 1.0*500 + 1e-7*500^3 + 200 = 540 + 500 + 12.5 + 200 = 1252.5 W
    HEA_CHECK_NEAR(kMotor.loss(60.0, 500.0), 1252.5, 1e-9);
    // Motor: P_e = 30000 + 1252.5,  jeneratör: P_e = -30000 + 1252.5
    HEA_CHECK_NEAR(kMotor.electricalPower(60.0, 500.0), 31252.5, 1e-9);
    HEA_CHECK_NEAR(kMotor.electricalPower(-60.0, 500.0), -28747.5, 1e-9);
    // Ters fonksiyon tutarlı olmalı
    HEA_CHECK_NEAR(kMotor.torqueForElectricalPower(31252.5, 500.0), 60.0, 1e-9);
    HEA_CHECK_NEAR(kMotor.torqueForElectricalPower(-28747.5, 500.0), -60.0, 1e-9);
}

static void testMotorEfficiencyShape()
{
    // Verim = P_mek / P_e. Orta yükte, düşük yükten yüksek olmalı.
    const double w = 500.0;
    auto eff = [&](double t) { return t * w / kMotor.electricalPower(t, w); };
    HEA_CHECK(eff(60.0) > eff(10.0));
    HEA_CHECK(eff(60.0) > 0.9);
    HEA_CHECK(eff(10.0) < 0.9);
}

// ---------------------------------------------------------------- İçten yanmalı motor

static void testEngineFullLoad()
{
    // 1500 dev/dak = 157.08 rad/s: 1000 (110 Nm) ile 2000 (130 Nm) arasının ortası -> 120 Nm
    HEA_CHECK_NEAR(kEngine.maxTorque(units::rpmToRadps(1500.0)), 120.0, 1e-3);
    HEA_CHECK_NEAR(kEngine.maxTorque(units::rpmToRadps(4000.0)), 142.0, 1e-3);
    // Tablo noktaları ve rölanti sınırı tam olarak kabul edilmeli (yuvarlama hatası yok)
    HEA_CHECK_NEAR(kEngine.maxTorque(units::rpmToRadps(1000.0)), 110.0, 1e-12);
    HEA_CHECK_NEAR(kEngine.maxTorque(units::rpmToRadps(3000.0)), 140.0, 1e-12);
    HEA_CHECK(kEngine.canDeliver(100.0, units::rpmToRadps(1000.0)));
    HEA_CHECK(kEngine.canDeliver(140.0, units::rpmToRadps(3000.0)));
    HEA_CHECK_NEAR(kEngine.maxTorque(units::rpmToRadps(800.0)), 0.0, 0.0);  // rölanti altı
    HEA_CHECK_NEAR(kEngine.maxTorque(units::rpmToRadps(6000.0)), 0.0, 0.0); // azami üstü
    // Tepe güç ~ 138 Nm * 523.6 rad/s = 72.3 kW (5000 dev/dak)
    HEA_CHECK_NEAR(kEngine.maxTorque(523.6) * 523.6 / 1000.0, 72.26, 0.05);
}

static void testEngineFuel()
{
    // 2000 dev/dak = 209.44 rad/s, T = 100 Nm:
    //   T_sur  = 12 + 0.02*209.44 = 16.1888 Nm
    //   P_yakit = (100 + 16.1888) * 209.44 / 0.40 = 60836.4 W
    //   m_dot  = 60836.4 / 43e6 = 1.41480e-3 kg/s (~5.1 kg/h)
    //   verim  = 100*209.44 / 60836.4 = 0.34427 -> BSFC = 3.6e9/(0.34427*43e6) = 243 g/kWh
    const double w = 209.44;
    HEA_CHECK_NEAR(kEngine.frictionTorque(w), 16.1888, 1e-9);
    HEA_CHECK_NEAR(kEngine.fuelPower(100.0, w), 60836.4, 0.1);
    HEA_CHECK_NEAR(kEngine.fuelRate(100.0, w), 1.41480e-3, 1e-8);
    HEA_CHECK_NEAR(kEngine.efficiency(100.0, w), 0.34427, 1e-5);
    // Sabit devirde yük arttıkça verim artar (sürtünme payı küçülür)
    HEA_CHECK(kEngine.efficiency(130.0, w) > kEngine.efficiency(100.0, w));
    HEA_CHECK(kEngine.efficiency(20.0, w) < 0.25);
    // Sıfır torkta bile sürtünmeyi yenmek için yakıt yakılır
    HEA_CHECK(kEngine.fuelRate(0.0, w) > 0.0);
}

// ---------------------------------------------------------------- Güç aktarma

static PowertrainRequest makeRequest(double wheelTorque, double wheelSpeed, Mode mode)
{
    PowertrainRequest r;
    r.wheelTorque_Nm = wheelTorque;
    r.wheelSpeed_radps = wheelSpeed;
    r.requestedMode = mode;
    r.batteryMaxDischarge_W = 25000.0;
    r.batteryMaxCharge_W = 25000.0;
    return r;
}

static void testStopped()
{
    const PowertrainResult r = kPt.resolve(makeRequest(0.0, 0.0, Mode::Electric));
    HEA_CHECK(r.mode == Mode::Stopped);
    HEA_CHECK_NEAR(r.fuelRate_kgps, 0.0, 0.0);
    HEA_CHECK_NEAR(r.motorElectricalPower_W, 0.0, 0.0);

    // Yokuşta duran aracı sürtünmeli fren tutar
    const PowertrainResult h = kPt.resolve(makeRequest(-200.0, 0.0, Mode::Engine));
    HEA_CHECK(h.mode == Mode::Stopped);
    HEA_CHECK_NEAR(h.frictionBrakeTorque_Nm, -200.0, 1e-12);
}

static void testElectricCruise()
{
    // 50 km/h, T_tekerlek = 60 Nm (F = 200 N):
    //   omega_motor = 46.2963 * 8 = 370.37 rad/s
    //   T_motor = 60 / (8 * 0.97) = 7.7320 Nm
    const PowertrainResult r = kPt.resolve(makeRequest(60.0, kWheel50, Mode::Electric));
    HEA_CHECK(r.mode == Mode::Electric);
    HEA_CHECK(!r.overridden);
    HEA_CHECK_NEAR(r.motorSpeed_radps, 370.370, 1e-3);
    HEA_CHECK_NEAR(r.motorTorque_Nm, 7.7320, 1e-4);
    HEA_CHECK_NEAR(r.motorElectricalPower_W,
                   kMotor.electricalPower(r.motorTorque_Nm, r.motorSpeed_radps), 1e-9);
    HEA_CHECK_NEAR(r.fuelRate_kgps, 0.0, 0.0);
    HEA_CHECK_NEAR(r.unmetWheelTorque_Nm, 0.0, 0.0);
    HEA_CHECK_NEAR(r.engineSpeed_radps, 0.0, 0.0); // motor kapalı
}

static void testElectricOverriddenWhenBatteryEmpty()
{
    PowertrainRequest req = makeRequest(60.0, kWheel50, Mode::Electric);
    req.batteryMaxDischarge_W = 0.0;
    const PowertrainResult r = kPt.resolve(req);
    HEA_CHECK(r.mode == Mode::Engine);
    HEA_CHECK(r.overridden);
    HEA_CHECK(r.fuelRate_kgps > 0.0);
    HEA_CHECK_NEAR(r.deliveredWheelTorque_Nm, 60.0, 1e-9);
}

static void testEngineCruiseGearChoice()
{
    // 100 km/h düz yol, F = 378 N -> T_tekerlek = 113.4 Nm, omega_tekerlek = 92.593 rad/s
    //   5. vites: G = 0.8*3.9 = 3.12 -> 288.9 rad/s (2759 dev/dak), T = 38.26 Nm -> P_yakit 40.5 kW
    //   4. vites: G = 3.9         -> 361.1 rad/s (3448 dev/dak), T = 30.61 Nm -> P_yakit 45.0 kW
    //   En düşük yakıt 5. viteste.
    const double tw = 113.4;
    const PowertrainResult r = kPt.resolve(makeRequest(tw, kWheel100, Mode::Engine));
    HEA_CHECK(r.mode == Mode::Engine);
    HEA_CHECK(r.gear == 5);
    HEA_CHECK(!r.clutchSlip);
    HEA_CHECK_NEAR(r.engineSpeed_radps, kWheel100 * 3.12, 1e-9);
    HEA_CHECK_NEAR(r.engineTorque_Nm, tw / (3.12 * 0.95), 1e-9);
    // Güç dengesi: motor gücü * vites verimi = tekerlek gücü
    HEA_CHECK_NEAR(r.engineTorque_Nm * r.engineSpeed_radps * 0.95, tw * kWheel100, 1e-6);
    HEA_CHECK_NEAR(r.motorElectricalPower_W, 0.0, 0.0);
}

static void testEngineCharging()
{
    PowertrainRequest req = makeRequest(113.4, kWheel100, Mode::Engine);
    const PowertrainResult plain = kPt.resolve(req);
    req.chargePower_W = 5000.0;
    const PowertrainResult charged = kPt.resolve(req);
    HEA_CHECK(charged.mode == Mode::Engine);
    HEA_CHECK(charged.motorTorque_Nm < 0.0);                    // jeneratör
    HEA_CHECK_NEAR(charged.motorElectricalPower_W, -5000.0, 1e-6); // tam istenen şarj
    HEA_CHECK(charged.fuelRate_kgps > plain.fuelRate_kgps);     // ek yük yakıta yansır
    HEA_CHECK_NEAR(charged.deliveredWheelTorque_Nm, 113.4, 1e-9);
}

static void testRegenLimitedByBattery()
{
    // 50 km/h, T_tekerlek = -900 Nm (F = -3000 N):
    //   T_motor = -900 * 0.97 / 8 = -109.125 Nm (sınır 143.1 Nm içinde)
    //   P_e = -109.125*370.37 + kayıplar(2361.7) = -38055 W -> batarya sınırı -25 kW
    const PowertrainResult r = kPt.resolve(makeRequest(-900.0, kWheel50, Mode::Electric));
    HEA_CHECK(r.mode == Mode::RegenBraking);
    HEA_CHECK_NEAR(r.motorElectricalPower_W, -25000.0, 1e-6);
    HEA_CHECK(r.frictionBrakeTorque_Nm < 0.0);
    // Tork dengesi: motorun tekerlekteki payı + sürtünme = istenen fren torku
    const double regenWheel = r.motorTorque_Nm * 8.0 / 0.97;
    HEA_CHECK_NEAR(regenWheel + r.frictionBrakeTorque_Nm, -900.0, 1e-9);
    HEA_CHECK_NEAR(r.fuelRate_kgps, 0.0, 0.0);
}

static void testRegenLightBrakingFullyElectric()
{
    // Hafif fren (-100 Nm): tamamı elektrik motoruyla, sürtünmeli fren yok
    const PowertrainResult r = kPt.resolve(makeRequest(-100.0, kWheel50, Mode::Engine));
    HEA_CHECK(r.mode == Mode::RegenBraking);
    HEA_CHECK_NEAR(r.frictionBrakeTorque_Nm, 0.0, 1e-9);
    HEA_CHECK_NEAR(r.motorTorque_Nm, -100.0 * 0.97 / 8.0, 1e-12);
    HEA_CHECK(r.motorElectricalPower_W < 0.0);
}

static void testRegenBelowMinSpeed()
{
    // 3 rad/s < 5 rad/s: tamamı sürtünmeli fren
    const PowertrainResult r = kPt.resolve(makeRequest(-300.0, 3.0, Mode::Electric));
    HEA_CHECK(r.mode == Mode::RegenBraking);
    HEA_CHECK_NEAR(r.motorTorque_Nm, 0.0, 0.0);
    HEA_CHECK_NEAR(r.frictionBrakeTorque_Nm, -300.0, 1e-12);
}

static void testEngineLaunchWithClutchSlip()
{
    // 0.3 m/s'de kalkış: 1. viteste motor devri 1.0*13.65 = 13.65 rad/s < rölanti
    //   -> kavrama kayar, motor rölantide (104.72 rad/s), T = 300 / (13.65*0.95) = 23.134 Nm
    const PowertrainResult r = kPt.resolve(makeRequest(300.0, 1.0, Mode::Engine));
    HEA_CHECK(r.mode == Mode::Engine);
    HEA_CHECK(r.gear == 1);
    HEA_CHECK(r.clutchSlip);
    HEA_CHECK_NEAR(r.engineSpeed_radps, 104.72, 1e-3);
    HEA_CHECK_NEAR(r.engineTorque_Nm, 300.0 / (13.65 * 0.95), 1e-9);
}

static void testEngineWithMotorAssist()
{
    // 100 km/h'de F = 3600 N (~100 kW): hiçbir viteste motor tek başına yetmez.
    //   En büyük eksen torku 3. viteste: 138.7 Nm * 5.46 * 0.95 = 719 Nm
    //   Kalan 361 Nm'yi elektrik motoru verir ama batarya 25 kW ile sınırlı -> eksik kalır.
    const double tw = 3600.0 * 0.30;
    const PowertrainResult r = kPt.resolve(makeRequest(tw, kWheel100, Mode::Engine));
    HEA_CHECK(r.mode == Mode::Engine);
    HEA_CHECK(r.gear == 3);
    HEA_CHECK_NEAR(r.engineTorque_Nm, kEngine.maxTorque(r.engineSpeed_radps), 1e-9);
    HEA_CHECK(r.motorTorque_Nm > 0.0);
    HEA_CHECK_NEAR(r.motorElectricalPower_W, 25000.0, 1e-6);
    HEA_CHECK(r.unmetWheelTorque_Nm > 0.0);
    HEA_CHECK_NEAR(r.deliveredWheelTorque_Nm + r.unmetWheelTorque_Nm, tw, 1e-9);
}

int main()
{
    testMotorParams();
    testMotorTorqueLimit();
    testMotorLossAndPower();
    testMotorEfficiencyShape();
    testEngineFullLoad();
    testEngineFuel();
    testStopped();
    testElectricCruise();
    testElectricOverriddenWhenBatteryEmpty();
    testEngineCruiseGearChoice();
    testEngineCharging();
    testRegenLimitedByBattery();
    testRegenLightBrakingFullyElectric();
    testRegenBelowMinSpeed();
    testEngineLaunchWithClutchSlip();
    testEngineWithMotorAssist();
    return HEA_TEST_RESULT();
}
