// Tüketim modeli (C++ tarafı) ve model tabanlı strateji testleri.

#include <cmath>

#include "hea/consumption_model.h"
#include "hea/drive_cycle.h"
#include "hea/model_based_strategy.h"
#include "hea/rule_based_strategy.h"
#include "hea/training_data.h"
#include "hea/units.h"
#include "test_util.h"

using namespace hea;

static void testModelLoads()
{
    const ConsumptionModel m = ConsumptionModel::fromGeneratedCoefficients();
    HEA_CHECK(m.valid());

    // Tanınmayan özellik adı modeli geçersiz kılar
    const char* const feats[] = {"P", "bilinmeyen"};
    const double coefs[] = {1.0, 2.0};
    const generated::LinearModelCoeffs bad = {"bozuk", feats, coefs, 2, 0.0};
    HEA_CHECK(!LinearModel(bad).valid());

    // y = 1 + 2*P + 3*w^2 elle kontrol
    const char* const f2[] = {"P", "w^2"};
    const double c2[] = {2.0, 3.0};
    const LinearModel lm(generated::LinearModelCoeffs{"elle", f2, c2, 2, 1.0});
    HEA_CHECK(lm.valid());
    HEA_CHECK_NEAR(lm.predict(10.0, 99.0, 4.0), 1.0 + 20.0 + 48.0, 1e-12);
}

static void testCppMatchesPythonAccuracy()
{
    // C++'taki tahminler, test çevrimlerinde Python'da ölçülen doğrulukla aynı olmalı.
    // (Aynı veri üretimi + aynı katsayılar -> aynı sonuç; aktarma hatası olmadığının kanıtı)
    const ConsumptionModel m = ConsumptionModel::fromGeneratedCoefficients();
    for (const DriveCycle& c : testCycles()) {
        RuleBasedStrategy driver;
        const auto samples = buildTrainingSamples(SimulationConfig{}, c, driver);
        double evErrMax = 0.0;
        double iceTrue = 0.0, icePred = 0.0;
        double chTrue = 0.0, chPred = 0.0;
        for (const TrainingSample& s : samples) {
            if (s.evFeasible) {
                evErrMax = std::max(evErrMax, std::fabs(m.evBatteryPower_W(s.in) - s.evBatteryPower_W));
            }
            if (s.iceFeasible) {
                iceTrue += s.iceFuelRate_kgps;
                icePred += m.iceFuelRate_kgps(s.in);
                if (s.iceChargePower_W > 500.0) {
                    chTrue += (s.iceChargeFuelRate_kgps - s.iceFuelRate_kgps) / s.iceChargePower_W;
                    chPred += m.chargeFuelPerJoule(s.in);
                }
            }
        }
        // EV modeli fiziği birebir buldu: hata 1 mW'ın altında
        HEA_CHECK(evErrMax < 1e-3);
        // ICE: çevrim toplamında %1'den az hata (Python: en kötü -%0.49)
        HEA_CHECK(std::fabs(icePred / iceTrue - 1.0) < 0.01);
        // Şarj maliyeti: çevrim toplamında %3'ten az hata (Python: en kötü +%1.58)
        HEA_CHECK(std::fabs(chPred / chTrue - 1.0) < 0.03);
    }
}

static void testEquivalenceFactor()
{
    const ModelBasedStrategy s;
    // s = 2.8 * (1 + 5 * (0.6 - SOC))
    HEA_CHECK_NEAR(s.equivalenceFactor(0.60), 2.8, 1e-12);
    HEA_CHECK_NEAR(s.equivalenceFactor(0.50), 4.2, 1e-12);
    HEA_CHECK_NEAR(s.equivalenceFactor(0.70), 1.4, 1e-12);
    HEA_CHECK_NEAR(s.equivalenceFactor(0.90), 0.0, 0.0); // negatif olamaz
}

static StrategyInput input(double t, double speedKmh, double forceN, double soc)
{
    StrategyInput in;
    in.time_s = t;
    in.speed_mps = units::kmhToMps(speedKmh);
    in.wheelSpeed_radps = in.speed_mps / 0.30;
    in.wheelTorque_Nm = forceN * 0.30;
    in.wheelPower_W = in.wheelTorque_Nm * in.wheelSpeed_radps;
    in.soc = soc;
    in.batteryMaxDischarge_W = 24000.0;
    in.batteryMaxCharge_W = 25000.0;
    return in;
}

static void testDecisionsFollowCosts()
{
    // Fren -> EV, maliyet hesaplanmaz
    {
        ModelBasedStrategy s;
        HEA_CHECK(s.decide(input(0, 50, -2000, 0.6)).mode == Mode::Electric);
    }
    // SOC koruma sınırının altında EV yasak
    {
        ModelBasedStrategy s;
        const Decision d = s.decide(input(0, 30, 300, 0.33));
        HEA_CHECK(d.mode == Mode::Engine);
        HEA_CHECK(!s.lastCosts().evAllowed);
    }
    // EV gücü batarya sınırını aşıyorsa EV yasak (100 km/h, 3000 N ~ 83 kW)
    {
        ModelBasedStrategy s;
        s.decide(input(0, 100, 3000, 0.6));
        HEA_CHECK(!s.lastCosts().evAllowed);
    }
    // Karar her zaman en küçük maliyetli seçenek (motor kapalıyken, ilk karar)
    for (double soc : {0.45, 0.55, 0.60, 0.65, 0.75}) {
        for (double kmh : {20.0, 50.0, 90.0}) {
            ModelBasedStrategy s;
            const Decision d = s.decide(input(0, kmh, 400, soc));
            const EcmsCosts& c = s.lastCosts();
            const double bestIce = std::min(c.ice, c.iceCharge);
            HEA_CHECK((d.mode == Mode::Engine) == (bestIce < c.ev));
        }
    }
}

static void testChargeOnlyBelowTarget()
{
    // SOC hedefin altında (s büyük) şarj kârlı; üstünde (s küçük) kârsız.
    // SOC 0.40: EV yasak değil (>= 0.35) ama s = 5.6 -> elektrik çok pahalı -> ICE + şarj
    ModelBasedStrategy low;
    const Decision dl = low.decide(input(0, 60, 500, 0.40));
    HEA_CHECK(dl.mode == Mode::Engine);
    HEA_CHECK_NEAR(dl.chargePower_W, 8000.0, 1e-9);

    // SOC 0.70, yüksek güç (ICE zorunlu değil ama pahalı EV): şarj istenmemeli
    ModelBasedStrategy high;
    const Decision dh = high.decide(input(0, 100, 900, 0.70));
    if (dh.mode == Mode::Engine) {
        HEA_CHECK_NEAR(dh.chargePower_W, 0.0, 0.0);
    }
}

static void testMinimumOnTime()
{
    ModelBasedStrategy s;
    // SOC düşük -> motor açılır
    HEA_CHECK(s.decide(input(0, 60, 500, 0.40)).mode == Mode::Engine);
    // Hemen ardından SOC yüksek ve düşük güç: EV ucuz olsa da 10 s dolmadan kapanmaz
    for (int t = 1; t < 10; ++t) {
        HEA_CHECK(s.decide(input(t, 30, 200, 0.75)).mode == Mode::Engine);
    }
    HEA_CHECK(s.decide(input(10, 30, 200, 0.75)).mode == Mode::Electric);
}

static void testChargeSustainingOnTrainingCycles()
{
    const SimulationConfig cfg;
    for (const DriveCycle& c : trainingCycles()) {
        ModelBasedStrategy s;
        const auto steps = runCycle(cfg, c, s);
        double socMin = 1.0;
        for (const StepResult& r : steps) {
            socMin = std::min(socMin, r.soc);
        }
        const RunSummary sum = summarize(steps, cfg.initialSoc);
        HEA_CHECK(socMin > 0.33);
        HEA_CHECK(std::fabs(sum.socEnd - 0.60) < 0.15);
        HEA_CHECK(sum.overriddenFraction < 0.05);
        HEA_CHECK_NEAR(sum.maxSpeedError_mps, 0.0, 1e-6);
        if (socMin <= 0.33 || std::fabs(sum.socEnd - 0.60) >= 0.15 || sum.overriddenFraction >= 0.05) {
            std::fprintf(stderr, "  %s: socMin=%.3f socEnd=%.3f uygulanamayan=%.3f\n",
                         c.name.c_str(), socMin, sum.socEnd, sum.overriddenFraction);
        }
    }
}

int main()
{
    HEA_CHECK(ModelBasedParams{}.isValid());
    testModelLoads();
    testCppMatchesPythonAccuracy();
    testEquivalenceFactor();
    testDecisionsFollowCosts();
    testChargeOnlyBelowTarget();
    testMinimumOnTime();
    testChargeSustainingOnTrainingCycles();
    return HEA_TEST_RESULT();
}
