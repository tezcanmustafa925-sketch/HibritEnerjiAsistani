// Simülasyon döngüsü testleri.

#include <cmath>

#include "hea/drive_cycle.h"
#include "hea/simulation.h"
#include "hea/units.h"
#include "test_util.h"

using namespace hea;

// Yardımcı yük kapalı yapılandırma: batarya yalnızca motor için kullanılır,
// böylece SOC değişimi doğrudan güç aktarma kararlarına bağlanabilir.
static SimulationConfig noAuxConfig()
{
    SimulationConfig c;
    c.auxiliaryPower_W = 0.0;
    return c;
}

static void testEngineCruise()
{
    // 0 -> 72 km/h (20 m/s), sonra 500 s sabit hız, düz yol, yalnızca ICE
    const DriveCycle cycle = CycleBuilder("seyir").rampTo(20.0, 1.0).hold(500.0).build();
    FixedModeStrategy ice(Mode::Engine);
    const auto steps = runCycle(noAuxConfig(), cycle, ice);

    HEA_CHECK(steps.size() == cycle.samples.size() - 1);
    // İdeal sürücü hedef hızı tam izler
    for (const StepResult& r : steps) {
        HEA_CHECK_NEAR(r.speed_mps, r.targetSpeed_mps, 1e-9);
        HEA_CHECK(r.mode == Mode::Engine);
        HEA_CHECK_NEAR(r.unmetWheelTorque_Nm, 0.0, 0.0);
    }
    // Yardımcı yük yok, şarj/rejen yok -> SOC değişmemeli
    HEA_CHECK_NEAR(steps.back().soc, 0.60, 1e-12);

    // Sabit 72 km/h'de: F = 129.4 (aero) + 128.5 (yuv) = 257.8 N, P = 5.16 kW
    //   5. vites: 208 rad/s (1986 dev/dak), T = 26.1 Nm -> P_yakit ~ 20.0 kW
    //   -> 0.465 g/s; 100 km = 5000 s -> 2.32 kg = 3.1 L/100 km
    const StepResult& cruise = steps.back();
    HEA_CHECK(cruise.gear == 5);
    const double lPer100 = cruise.fuelRate_kgps * 5000.0 / constants::kGasolineDensity;
    HEA_CHECK_NEAR(lPer100, 3.12, 0.05);
}

static void testHoldOnHill()
{
    // %5 yokuşta 10 s bekleme: fren aracı tutar, enerji harcanmaz
    const DriveCycle cycle = CycleBuilder("tepe").hold(10.0).grade({{0.0, 5.0}}).build();
    FixedModeStrategy ev(Mode::Electric);
    const auto steps = runCycle(noAuxConfig(), cycle, ev);
    for (const StepResult& r : steps) {
        HEA_CHECK(r.mode == Mode::Stopped);
        HEA_CHECK_NEAR(r.speed_mps, 0.0, 0.0);
        HEA_CHECK_NEAR(r.batteryCurrent_A, 0.0, 0.0);
        HEA_CHECK_NEAR(r.fuelRate_kgps, 0.0, 0.0);
        HEA_CHECK(r.gradeForce_N > 0.0);
    }
}

static void testRegenOnDeceleration()
{
    const DriveCycle cycle =
        CycleBuilder("rejen").rampTo(15.0, 1.0).hold(10.0).rampTo(0.0, 1.5).hold(3.0).build();
    FixedModeStrategy ev(Mode::Electric);
    const auto steps = runCycle(noAuxConfig(), cycle, ev);

    int regenSteps = 0;
    for (const StepResult& r : steps) {
        if (r.acceleration_mps2 < -1.0 && r.speed_mps > 2.0) {
            HEA_CHECK(r.mode == Mode::RegenBraking);
            HEA_CHECK(r.batteryCurrent_A < 0.0); // şarj
            HEA_CHECK(r.brake > 0.0);
            HEA_CHECK_NEAR(r.throttle, 0.0, 0.0);
            ++regenSteps;
        }
        if (r.acceleration_mps2 > 0.5) {
            // 1 m/s^2 ile ~47 km/h üstünde gereken güç (~27 kW) bataryanın
            // ~25 kW sınırını aşar: güç aktarma organı ICE'ye geçer (overridden).
            HEA_CHECK(r.mode == Mode::Electric || (r.mode == Mode::Engine && r.modeOverridden));
            HEA_CHECK(r.throttle > 0.0);
            HEA_CHECK_NEAR(r.brake, 0.0, 0.0);
        }
        // Yakıt yalnızca EV önerisi uygulanamadığında yakılır
        if (r.fuelRate_kgps > 0.0) {
            HEA_CHECK(r.modeOverridden);
        }
    }
    HEA_CHECK(regenSteps > 5);
    const RunSummary s = summarize(steps, 0.60);
    HEA_CHECK(s.regenEnergy_J > 0.0);
    // Sabit hız bölümü (54 km/h, ~6 kW) tamamen elektrikle gider
    HEA_CHECK(steps[20].mode == Mode::Electric);
}

static void testEvPreferredFallsBackOnlyWhenNeeded()
{
    // EV öncelikli stratejiyle uzun karma çevrim: yakıt yalnızca öneri
    // uygulanamadığında (overridden) yakılmalı; SOC alt sınırın altına inmemeli.
    const DriveCycle cycle = makeMixedCycle(21);
    FixedModeStrategy ev(Mode::Electric);
    const SimulationConfig cfg; // yardımcı yük açık
    const auto steps = runCycle(cfg, cycle, ev);
    bool anyOverride = false;
    for (const StepResult& r : steps) {
        if (r.fuelRate_kgps > 0.0) {
            HEA_CHECK(r.modeOverridden);
        }
        anyOverride = anyOverride || r.modeOverridden;
        HEA_CHECK(r.soc >= cfg.battery.socMin - 0.01);
        HEA_CHECK(r.throttle >= 0.0 && r.throttle <= 1.0);
        HEA_CHECK(r.brake >= 0.0 && r.brake <= 1.0);
    }
    // 20 km'lik tepelik çevrim ~0.4 kWh'lik kullanılabilir bataryayı tüketir
    HEA_CHECK(anyOverride);
}

static void testUnmetTorqueVehicleLags()
{
    // 0 -> 30 m/s 3 saniyede (10 m/s^2): araç bunu yapamaz, geride kalır,
    // sonra sabit hız bölümünde hedefi yakalar.
    const DriveCycle cycle = CycleBuilder("sert").rampTo(30.0, 10.0).hold(90.0).build();
    FixedModeStrategy ice(Mode::Engine);
    const auto steps = runCycle(noAuxConfig(), cycle, ice);
    bool lagged = false;
    for (const StepResult& r : steps) {
        HEA_CHECK(r.speed_mps <= r.targetSpeed_mps + 1e-9); // hedefi aşmaz
        if (r.targetSpeed_mps - r.speed_mps > 1.0) {
            lagged = true;
            HEA_CHECK(r.unmetWheelTorque_Nm > 0.0);
            HEA_CHECK_NEAR(r.throttle, 1.0, 1e-12); // gaz sonuna kadar
        }
    }
    HEA_CHECK(lagged);
    HEA_CHECK_NEAR(steps.back().speed_mps, 30.0, 1e-9);
}

static void testTrackingAndDistance()
{
    // Normal şehir çevriminde araç hedefi tam izler, yol çevrimin yoluna eşittir
    const DriveCycle cycle = makeUrbanCycle(2);
    FixedModeStrategy ice(Mode::Engine);
    const auto steps = runCycle(SimulationConfig{}, cycle, ice);
    const RunSummary s = summarize(steps, 0.60);
    HEA_CHECK_NEAR(s.maxSpeedError_mps, 0.0, 1e-9);
    HEA_CHECK_NEAR(s.distance_m, cycle.distance_m(), 1e-6);
    HEA_CHECK_NEAR(s.duration_s, cycle.duration_s(), 1e-9);
    // Mod oranlarının toplamı 1
    HEA_CHECK_NEAR(s.timeElectric + s.timeEngine + s.timeRegen + s.timeStopped, 1.0, 1e-12);
    // Şehir içi sadece-ICE: makul tüketim aralığı
    HEA_CHECK(s.fuelPer100km_L > 3.0 && s.fuelPer100km_L < 8.0);
}

static void testDeterminism()
{
    const DriveCycle cycle = makeHighwayCycle(11);
    FixedModeStrategy a(Mode::Engine);
    FixedModeStrategy b(Mode::Engine);
    const auto s1 = runCycle(SimulationConfig{}, cycle, a);
    const auto s2 = runCycle(SimulationConfig{}, cycle, b);
    HEA_CHECK(s1.back().fuelUsed_kg == s2.back().fuelUsed_kg);
    HEA_CHECK(s1.back().soc == s2.back().soc);
}

static void testBatteryEnergyAccounting()
{
    // Birikimli batarya enerjisi = sum(P_batarya * dt)
    const DriveCycle cycle = makeUrbanCycle(3);
    FixedModeStrategy ev(Mode::Electric);
    const auto steps = runCycle(SimulationConfig{}, cycle, ev);
    double sum = 0.0;
    for (const StepResult& r : steps) {
        sum += r.batteryPower_W * r.dt_s;
    }
    HEA_CHECK_NEAR(steps.back().batteryEnergyUsed_J, sum, 1e-6);
}

static void testSummaryArithmetic()
{
    // Elle kurulmuş iki adım: 1000 m'de 0.0745 kg yakıt = 0.1 L -> 10 L/100 km
    std::vector<StepResult> steps(2);
    steps[0].dt_s = 1.0;
    steps[0].mode = Mode::Engine;
    steps[1].dt_s = 3.0;
    steps[1].mode = Mode::Electric;
    steps[1].distance_m = 1000.0;
    steps[1].fuelUsed_kg = 0.0745;
    steps[1].soc = 0.55;
    const RunSummary s = summarize(steps, 0.60);
    HEA_CHECK_NEAR(s.duration_s, 4.0, 0.0);
    HEA_CHECK_NEAR(s.fuel_L, 0.1, 1e-12);
    HEA_CHECK_NEAR(s.fuelPer100km_L, 10.0, 1e-9);
    HEA_CHECK_NEAR(s.timeEngine, 0.25, 1e-12);
    HEA_CHECK_NEAR(s.timeElectric, 0.75, 1e-12);
    HEA_CHECK_NEAR(s.socEnd, 0.55, 0.0);
}

int main()
{
    testEngineCruise();
    testHoldOnHill();
    testRegenOnDeceleration();
    testEvPreferredFallsBackOnlyWhenNeeded();
    testUnmetTorqueVehicleLags();
    testTrackingAndDistance();
    testDeterminism();
    testBatteryEnergyAccounting();
    testSummaryArithmetic();
    return HEA_TEST_RESULT();
}
