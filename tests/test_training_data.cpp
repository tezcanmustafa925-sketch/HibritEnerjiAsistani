// Eğitim verisi üretimi testleri.

#include <cmath>

#include "hea/drive_cycle.h"
#include "hea/rule_based_strategy.h"
#include "hea/training_data.h"
#include "test_util.h"

using namespace hea;

static void testRecordingStrategyDelegates()
{
    // Kaydedici, sarılan stratejinin kararını değiştirmemeli
    const DriveCycle c = makeUrbanCycle(1);
    const SimulationConfig cfg;
    RuleBasedStrategy plain;
    RuleBasedStrategy inner;
    RecordingStrategy rec(inner);
    const auto a = runCycle(cfg, c, plain);
    const auto b = runCycle(cfg, c, rec);
    HEA_CHECK(rec.inputs().size() == a.size()); // her adımda bir karar
    HEA_CHECK(a.back().fuelUsed_kg == b.back().fuelUsed_kg);
    HEA_CHECK(a.back().soc == b.back().soc);
}

static void testOnlyTractionSamples()
{
    RuleBasedStrategy driver;
    const auto samples = buildTrainingSamples(SimulationConfig{}, makeMixedCycle(21), driver);
    HEA_CHECK(!samples.empty());
    for (const TrainingSample& s : samples) {
        HEA_CHECK(s.in.wheelPower_W > 0.0);
        HEA_CHECK(s.cycle == "karma_21");
        // ICE her zaman bir yakıt maliyeti verir (tahrik varken yakıt kesme olmaz)
        HEA_CHECK(s.iceFuelRate_kgps > 0.0);
        // EV mümkünse bataryadan güç çekilir
        if (s.evFeasible) {
            HEA_CHECK(s.evBatteryPower_W > 0.0);
        }
        // Şarj yalnızca ek yakıtla yapılabilir
        if (s.iceChargePower_W > 0.0) {
            HEA_CHECK(s.iceChargeFuelRate_kgps > s.iceFuelRate_kgps);
            HEA_CHECK(s.iceChargePower_W <= kTrainingChargeRequest_W + 1e-6);
        }
    }
}

static void testCounterfactualsMatchActualRuns()
{
    // Sabit EV stratejisiyle sürülen adımlarda karşı-olgusal EV gücü, simülasyonda
    // gerçekleşen motor gücüne eşit olmalı (aynı talep, aynı batarya sınırları).
    const DriveCycle c = makeUrbanCycle(2);
    const SimulationConfig cfg;
    {
        FixedModeStrategy ev(Mode::Electric);
        const auto steps = runCycle(cfg, c, ev);
        FixedModeStrategy ev2(Mode::Electric);
        const auto samples = buildTrainingSamples(cfg, c, ev2);
        int matched = 0;
        std::size_t k = 0;
        for (const StepResult& r : steps) {
            if (k < samples.size() && samples[k].in.time_s == r.time_s) {
                if (r.mode == Mode::Electric) {
                    HEA_CHECK(samples[k].evFeasible);
                    HEA_CHECK_NEAR(samples[k].evBatteryPower_W, r.motorElectricalPower_W, 1e-9);
                    ++matched;
                }
                ++k;
            }
        }
        HEA_CHECK(k == samples.size()); // her örnek bir adıma karşılık gelir
        HEA_CHECK(matched > 100);
    }
    {
        // Sabit ICE: karşı-olgusal yakıt = gerçekleşen yakıt (takviyesiz adımlarda)
        FixedModeStrategy ice(Mode::Engine);
        const auto steps = runCycle(cfg, c, ice);
        FixedModeStrategy ice2(Mode::Engine);
        const auto samples = buildTrainingSamples(cfg, c, ice2);
        int matched = 0;
        std::size_t k = 0;
        for (const StepResult& r : steps) {
            if (k < samples.size() && samples[k].in.time_s == r.time_s) {
                if (samples[k].iceFeasible) {
                    HEA_CHECK_NEAR(samples[k].iceFuelRate_kgps, r.fuelRate_kgps, 1e-15);
                    HEA_CHECK(samples[k].iceGear == r.gear);
                    ++matched;
                }
                ++k;
            }
        }
        HEA_CHECK(matched > 100);
    }
}

static void testDeterministic()
{
    RuleBasedStrategy a;
    RuleBasedStrategy b;
    const auto s1 = buildTrainingSamples(SimulationConfig{}, makeHighwayCycle(11), a);
    const auto s2 = buildTrainingSamples(SimulationConfig{}, makeHighwayCycle(11), b);
    HEA_CHECK(s1.size() == s2.size());
    HEA_CHECK(s1.back().iceFuelRate_kgps == s2.back().iceFuelRate_kgps);
}

int main()
{
    testRecordingStrategyDelegates();
    testOnlyTractionSamples();
    testCounterfactualsMatchActualRuns();
    testDeterministic();
    return HEA_TEST_RESULT();
}
