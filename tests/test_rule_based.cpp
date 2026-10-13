// Kural tabanlı strateji ve motor çalıştırma cezası testleri.

#include <cmath>

#include "hea/drive_cycle.h"
#include "hea/rule_based_strategy.h"
#include "hea/simulation.h"
#include "hea/units.h"
#include "test_util.h"

using namespace hea;

static StrategyInput input(double t, double speedKmh, double powerW, double soc)
{
    StrategyInput in;
    in.time_s = t;
    in.speed_mps = units::kmhToMps(speedKmh);
    in.wheelPower_W = powerW;
    in.soc = soc;
    return in;
}

static void testParams()
{
    HEA_CHECK(RuleBasedParams{}.isValid());
    RuleBasedParams bad;
    bad.socLow = 0.65; // hedefin üstünde
    HEA_CHECK(!bad.isValid());
}

static void testBasicRules()
{
    // Düşük hız, düşük güç, normal SOC -> EV
    {
        RuleBasedStrategy s;
        const Decision d = s.decide(input(0, 30, 5000, 0.60));
        HEA_CHECK(d.mode == Mode::Electric);
    }
    // Yüksek hız -> ICE
    {
        RuleBasedStrategy s;
        HEA_CHECK(s.decide(input(0, 90, 5000, 0.60)).mode == Mode::Engine);
    }
    // Yüksek güç (15 kW > 12 kW) -> ICE
    {
        RuleBasedStrategy s;
        HEA_CHECK(s.decide(input(0, 30, 15000, 0.60)).mode == Mode::Engine);
    }
    // Aynı güç ama SOC yüksek (0.75 > 0.70): eşik 20 kW -> EV
    {
        RuleBasedStrategy s;
        HEA_CHECK(s.decide(input(0, 30, 15000, 0.75)).mode == Mode::Electric);
    }
    // Düşük SOC -> düşük hız/güçte bile ICE + şarj
    //   P_sarj = 40000 * (0.60 - 0.40) = 8000 W (azami 8000 W)
    {
        RuleBasedStrategy s;
        const Decision d = s.decide(input(0, 20, 2000, 0.40));
        HEA_CHECK(d.mode == Mode::Engine);
        HEA_CHECK_NEAR(d.chargePower_W, 8000.0, 1e-9);
    }
    // Fren -> EV (rejeneratif)
    {
        RuleBasedStrategy s;
        HEA_CHECK(s.decide(input(0, 60, -10000, 0.40)).mode == Mode::Electric);
    }
}

static void testChargeProportionalToSocError()
{
    // Motor hız yüzünden açık, SOC 0.55: P_sarj = 40000 * 0.05 = 2000 W
    RuleBasedStrategy s;
    HEA_CHECK_NEAR(s.decide(input(0, 90, 10000, 0.55)).chargePower_W, 2000.0, 1e-9);
    // SOC hedefin üstünde: şarj yok
    RuleBasedStrategy s2;
    HEA_CHECK_NEAR(s2.decide(input(0, 90, 10000, 0.65)).chargePower_W, 0.0, 0.0);
}

static void testMinimumOnTimeAndHysteresis()
{
    RuleBasedStrategy s;
    // t=0: 60 km/h -> motor açılır
    HEA_CHECK(s.decide(input(0, 60, 8000, 0.60)).mode == Mode::Engine);
    // t=1..9: hız 30 km/h'ye düştü ama 10 s dolmadı -> motor açık kalır
    for (int t = 1; t < 10; ++t) {
        HEA_CHECK(s.decide(input(t, 30, 3000, 0.60)).mode == Mode::Engine);
    }
    // t=10: süre doldu, kapatma koşulları sağlanıyor -> EV
    HEA_CHECK(s.decide(input(10, 30, 3000, 0.60)).mode == Mode::Electric);

    // Hız histerezisi: 48 km/h (> 50 - 5) iken motor kapanmaz
    RuleBasedStrategy h;
    h.decide(input(0, 60, 8000, 0.60));
    HEA_CHECK(h.decide(input(20, 48, 3000, 0.60)).mode == Mode::Engine);
    HEA_CHECK(h.decide(input(21, 44, 3000, 0.60)).mode == Mode::Electric);
}

static void testReset()
{
    RuleBasedStrategy s;
    s.decide(input(0, 90, 8000, 0.60));
    HEA_CHECK(s.engineOn());
    s.reset();
    HEA_CHECK(!s.engineOn());
}

static void testChargeSustainingOnCycles()
{
    // Kural tabanlı strateji SOC'yi hedef etrafında tutmalı (şarj sürdürme)
    const SimulationConfig cfg;
    for (const DriveCycle& c : trainingCycles()) {
        RuleBasedStrategy s;
        const auto steps = runCycle(cfg, c, s);
        double socMin = 1.0;
        for (const StepResult& r : steps) {
            socMin = std::min(socMin, r.soc);
        }
        const RunSummary sum = summarize(steps, cfg.initialSoc);
        HEA_CHECK(socMin > 0.38);                       // alt sınıra (0.30) yaklaşmaz
        HEA_CHECK(std::fabs(sum.socEnd - 0.60) < 0.15); // hedeften fazla uzaklaşmaz
        HEA_CHECK(sum.overriddenFraction < 0.05);       // öneriler neredeyse hep uygulanabilir
        HEA_CHECK_NEAR(sum.maxSpeedError_mps, 0.0, 1e-6);
        if (std::fabs(sum.socEnd - 0.60) >= 0.15 || socMin <= 0.38) {
            std::fprintf(stderr, "  %s: socMin=%.3f socEnd=%.3f\n", c.name.c_str(), socMin,
                         sum.socEnd);
        }
    }
}

static void testEngineStartPenalty()
{
    // Ceza açıkken ve kapalıyken yakıt farkı = çalıştırma sayısı * 0.2 g
    const DriveCycle c = makeUrbanCycle(1);
    SimulationConfig withPenalty;
    SimulationConfig noPenalty;
    noPenalty.engineStartFuel_kg = 0.0;
    RuleBasedStrategy a;
    RuleBasedStrategy b;
    const RunSummary s1 = summarize(runCycle(withPenalty, c, a), 0.60);
    const RunSummary s2 = summarize(runCycle(noPenalty, c, b), 0.60);
    HEA_CHECK(s1.engineStarts > 0);
    HEA_CHECK(s1.engineStarts == s2.engineStarts);
    HEA_CHECK_NEAR(s1.fuel_kg - s2.fuel_kg, s1.engineStarts * 0.2e-3, 1e-12);
}

int main()
{
    testParams();
    testBasicRules();
    testChargeProportionalToSocError();
    testMinimumOnTimeAndHysteresis();
    testReset();
    testChargeSustainingOnCycles();
    testEngineStartPenalty();
    return HEA_TEST_RESULT();
}
