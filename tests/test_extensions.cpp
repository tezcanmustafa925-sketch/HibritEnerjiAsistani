// Prototip genişletmelerinin testleri: doğrulama, enerji dengesi, motor ısınması,
// batarya RC dalı, klima yükü, frende motorun açık kalması, dt != 1 s.

#include <cmath>
#include <stdexcept>

#include "hea/battery.h"
#include "hea/drive_cycle.h"
#include "hea/engine.h"
#include "hea/model_based_strategy.h"
#include "hea/rule_based_strategy.h"
#include "hea/simulation.h"
#include "hea/units.h"
#include "test_util.h"

using namespace hea;

template <typename E, typename F>
static bool throws(F&& f)
{
    try {
        f();
    } catch (const E&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

static void testValidation()
{
    const DriveCycle c = CycleBuilder("kisa").rampTo(10.0, 1.0).hold(5).build();
    FixedModeStrategy ice(Mode::Engine);

    HEA_CHECK(SimulationConfig{}.isValid());
    SimulationConfig badMass;
    badMass.vehicle.curbMass_kg = -1.0;
    HEA_CHECK(!badMass.isValid());
    HEA_CHECK(throws<std::invalid_argument>([&] { Simulation s(badMass, c, ice); }));

    SimulationConfig badSoc;
    badSoc.initialSoc = 0.9; // pencere 0.30-0.80
    HEA_CHECK(throws<std::invalid_argument>([&] { runCycle(badSoc, c, ice); }));

    SimulationConfig badEngine;
    badEngine.engine.fullLoadTorques_Nm.pop_back(); // devir/tork sayısı uyuşmuyor
    HEA_CHECK(throws<std::invalid_argument>([&] { runCycle(badEngine, c, ice); }));

    DriveCycle zeroDt = c;
    zeroDt.dt_s = 0.0;
    HEA_CHECK(throws<std::invalid_argument>([&] { runCycle(SimulationConfig{}, zeroDt, ice); }));

    // Bitmiş simülasyonda step() hata verir (dizi dışına okumaz)
    Simulation sim(SimulationConfig{}, c, ice);
    while (!sim.finished()) sim.step();
    HEA_CHECK(throws<std::logic_error>([&] { sim.step(); }));

    // Boş çevrim: hemen biter, özet sıfır
    DriveCycle empty;
    empty.name = "bos";
    HEA_CHECK(runCycle(SimulationConfig{}, empty, ice).empty());

    // Stratejiler geçersiz parametreyle kurulamaz
    RuleBasedParams rp;
    rp.socLow = 0.9;
    HEA_CHECK(throws<std::invalid_argument>([&] { RuleBasedStrategy s(rp); }));
    ModelBasedParams mp;
    mp.minEngineOnTime_s = 0.0;
    HEA_CHECK(throws<std::invalid_argument>([&] { ModelBasedStrategy s(mp); }));
    // Geçersiz tüketim modeliyle (bilinmeyen özellik) model stratejisi kurulamaz
    HEA_CHECK(throws<std::invalid_argument>([&] { ModelBasedStrategy s(ModelBasedParams{}, ConsumptionModel{}); }));
}

static void testHvac()
{
    HvacParams h;
    HEA_CHECK_NEAR(h.power_W(22.0), 0.0, 0.0);   // konfor
    HEA_CHECK_NEAR(h.power_W(24.9), 0.0, 0.0);   // bant içinde
    HEA_CHECK_NEAR(h.power_W(32.0), 1000.0, 1e-9); // 100 W/K * 10 K
    HEA_CHECK_NEAR(h.power_W(0.0), 660.0, 1e-9);   // 30 W/K * 22 K
    HEA_CHECK_NEAR(h.power_W(60.0), 3000.0, 1e-9); // en fazla 3 kW
    h.enabled = false;
    HEA_CHECK_NEAR(h.power_W(40.0), 0.0, 0.0);

    // Simülasyonda yardımcı yüke eklenir
    const DriveCycle hot = CycleBuilder("sicak").hold(10).ambient(32.0).build();
    FixedModeStrategy ev(Mode::Electric);
    const auto steps = runCycle(SimulationConfig{}, hot, ev);
    HEA_CHECK_NEAR(steps.back().auxiliaryPower_W, 1300.0, 1e-9); // 300 + 1000
}

static void testEngineWarmup()
{
    const Engine e{EngineParams{}};
    const double w = units::rpmToRadps(2000.0);
    // Sıcak motorda çarpan tam 1, 20 °C'de 1.6
    HEA_CHECK_NEAR(e.coldFrictionFactor(kEngineWarmTemp_C), 1.0, 0.0);
    HEA_CHECK_NEAR(e.coldFrictionFactor(20.0), 1.6, 1e-12);
    HEA_CHECK(e.fuelRate(50.0, w, 20.0) > e.fuelRate(50.0, w));
    // Isıl model: yakıt yakan motor ısınır, termostat sıcaklığını aşmaz
    double t = 20.0;
    for (int i = 0; i < 5000; ++i) {
        t = e.nextTemperature(t, 30000.0, 20.0, 1.0);
    }
    HEA_CHECK_NEAR(t, kEngineWarmTemp_C, 1e-9);
    // Kapalı motor ortama doğru soğur
    HEA_CHECK(e.nextTemperature(90.0, 0.0, 20.0, 60.0) < 90.0);

    // Aynı çevrimde soğuk başlangıç daha çok yakıt harcar
    const DriveCycle c = makeUrbanCycle(1);
    SimulationConfig cold;
    SimulationConfig warm;
    warm.engineColdStart = false;
    FixedModeStrategy a(Mode::Engine);
    FixedModeStrategy b(Mode::Engine);
    const RunSummary sc = summarize(runCycle(cold, c, a), 0.6);
    const RunSummary sw = summarize(runCycle(warm, c, b), 0.6);
    HEA_CHECK(sc.fuel_kg > sw.fuel_kg);
    // Soğuk çalıştırma cezası makul aralıkta (%2-%25)
    HEA_CHECK(sc.fuel_kg / sw.fuel_kg > 1.02 && sc.fuel_kg / sw.fuel_kg < 1.25);
}

static void testBatteryPolarization()
{
    // Sabit güçte V1, R1*I'ya üstel olarak oturur (tau = 20 s; 100 s'de %99);
    // 5 kW * 100 s SOC'yi ~0.1 düşürür, pencere içinde kalır.
    const BatteryParams p{};
    Battery b(p, 0.6, 25.0);
    const BatteryStep first = b.step(5000.0, 1.0, 25.0);
    BatteryStep last = first;
    for (int i = 0; i < 100; ++i) {
        last = b.step(5000.0, 1.0, 25.0);
    }
    HEA_CHECK(last.polarization_V > 0.0);
    HEA_CHECK_NEAR(last.polarization_V, p.polarizationResistance_Ohm * last.current_A, 0.05);
    // Aynı güç için gerilim düşünce akım artar
    HEA_CHECK(last.current_A > first.current_A);
    // Enerji dengesi her adımda korunur: OCV*I = P + kayıp
    HEA_CHECK_NEAR(last.ocv_V * last.current_A, last.power_W + last.heat_W, 1e-6);
    // Akım kesilince V1 söner (tau = 20 s; 200 s sonra ~0)
    for (int i = 0; i < 200; ++i) {
        last = b.step(0.0, 1.0, 25.0);
    }
    HEA_CHECK(std::fabs(last.polarization_V) < 1e-3);

    // R1 = 0 ise eski Rint modeline eşdeğer
    BatteryParams rint = p;
    rint.polarizationResistance_Ohm = 0.0;
    Battery r(rint, 0.6, 25.0);
    for (int i = 0; i < 10; ++i) r.step(10000.0, 1.0, 25.0);
    HEA_CHECK_NEAR(r.polarizationVoltage(), 0.0, 0.0);
}

static void testAuxiliaryEnergyBalance()
{
    // Batarya alt sınırda başlar; EV öncelikli strateji şarj etmez. Yardımcı yük
    // bataryadan karşılanamaz: ya jeneratörle üretilir ya da raporlanır, kaybolmaz.
    SimulationConfig cfg;
    cfg.initialSoc = cfg.battery.socMin;
    cfg.auxiliaryPower_W = 1000.0;
    // Duruş: motor yok, yük karşılanamaz -> raporlanır
    const DriveCycle idle = CycleBuilder("bekle").hold(30).build();
    FixedModeStrategy ev(Mode::Electric);
    const RunSummary si = summarize(runCycle(cfg, idle, ev), cfg.initialSoc);
    HEA_CHECK_NEAR(si.unmetElectricalEnergy_J, 30.0 * 1000.0, 1e-6);
    HEA_CHECK_NEAR(si.auxiliaryEnergy_J, 30.0 * 1000.0, 1e-6);

    // Seyir: güç aktarma organı ICE'ye geçer, jeneratör yardımcı yükü üretir
    const DriveCycle cruise = CycleBuilder("seyir").rampTo(15.0, 1.0).hold(60).build();
    FixedModeStrategy ice(Mode::Engine);
    const auto steps = runCycle(cfg, cruise, ice);
    double unmetCruise = 0.0;
    for (std::size_t i = 20; i < steps.size(); ++i) {
        unmetCruise += steps[i].unmetElectricalPower_W * steps[i].dt_s;
        HEA_CHECK(steps[i].motorElectricalPower_W < 0.0); // jeneratör
    }
    HEA_CHECK_NEAR(unmetCruise, 0.0, 1e-6);
}

static void testEngineStaysOnDuringShortBraking()
{
    // Motor açıldıktan hemen sonra kısa bir frenleme: asgari süre dolmadığı için
    // motor kapanmaz, frenden sonra yeniden çalıştırma sayılmaz.
    RuleBasedStrategy s;
    StrategyInput drive;
    drive.speed_mps = units::kmhToMps(60);
    drive.wheelPower_W = 8000.0;
    drive.soc = 0.60;
    StrategyInput brake = drive;
    brake.wheelPower_W = -5000.0;
    drive.time_s = 0;
    HEA_CHECK(s.decide(drive).mode == Mode::Engine);
    brake.time_s = 1;
    HEA_CHECK(s.decide(brake).mode == Mode::Engine); // yakıt kesmede açık
    HEA_CHECK(s.engineOn());
    // Asgari süre dolduktan sonra frende kapanır
    brake.time_s = 20;
    HEA_CHECK(s.decide(brake).mode == Mode::Electric);
    HEA_CHECK(!s.engineOn());
    // Araç durduysa süre dolmasa da kapanır
    RuleBasedStrategy s2;
    s2.decide(drive);
    StrategyInput stop = brake;
    stop.time_s = 1;
    stop.speed_mps = 0.0;
    HEA_CHECK(s2.decide(stop).mode == Mode::Electric);

    // Simülasyonda: fren anında yakıt yok, motor dönüyor, çalıştırma sayısı artmıyor
    // 0.3 m/s^2 ile hızlanma EV güç eşiğinin altında; motor 50 km/h'yi geçince açılır,
    // 2-3 s sonra kısa bir yavaşlama (fren) gelir, sonra 52 km/h'de seyir (motor gerekli).
    const DriveCycle c = CycleBuilder("fren").rampTo(15.0, 0.3).hold(2).rampTo(14.5, 0.5)
                             .hold(20).build();
    RuleBasedStrategy rb;
    const auto steps = runCycle(SimulationConfig{}, c, rb);
    int starts = 0;
    for (const StepResult& r : steps) {
        starts += r.engineStarted ? 1 : 0;
        if (r.mode == Mode::RegenBraking && r.requestedMode == Mode::Engine) {
            HEA_CHECK(r.engineRunning);
            HEA_CHECK_NEAR(r.fuelRate_kgps, 0.0, 0.0);
        }
    }
    HEA_CHECK(starts == 1);
}

static void testHalfSecondTimeStep()
{
    // dt = 0.5 s: ideal sürücü hedefi izler, yol çevrimin yoluna eşit
    const DriveCycle c = CycleBuilder("yarim", 0.5).hold(5).rampTo(15.0, 1.0).hold(30)
                             .rampTo(0.0, 1.0).hold(5).build();
    RuleBasedStrategy s;
    const auto steps = runCycle(SimulationConfig{}, c, s);
    HEA_CHECK(steps.size() == c.samples.size() - 1);
    const RunSummary sum = summarize(steps, 0.6);
    HEA_CHECK_NEAR(sum.distance_m, c.distance_m(), 1e-6);
    HEA_CHECK_NEAR(sum.duration_s, c.duration_s(), 1e-9);
    HEA_CHECK_NEAR(sum.maxSpeedError_mps, 0.0, 1e-9);
    for (const StepResult& r : steps) {
        HEA_CHECK_NEAR(r.dt_s, 0.5, 0.0);
    }
}

int main()
{
    testValidation();
    testHvac();
    testEngineWarmup();
    testBatteryPolarization();
    testAuxiliaryEnergyBalance();
    testEngineStaysOnDuringShortBraking();
    testHalfSecondTimeStep();
    return HEA_TEST_RESULT();
}
