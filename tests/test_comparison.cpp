// SOC düzeltmeli karşılaştırma testleri.

#include <algorithm>
#include <cmath>

#include "hea/comparison.h"
#include "hea/drive_cycle.h"
#include "hea/model_based_strategy.h"
#include "hea/rule_based_strategy.h"
#include "test_util.h"

using namespace hea;

static void testFitLine()
{
    // y = 3 + 2x tam doğru: R^2 = 1
    const LineFit f = fitLine({0, 1, 2, 3}, {3, 5, 7, 9});
    HEA_CHECK_NEAR(f.intercept, 3.0, 1e-12);
    HEA_CHECK_NEAR(f.slope, 2.0, 1e-12);
    HEA_CHECK_NEAR(f.r2, 1.0, 1e-12);

    // Elle: x = {0, 1, 2}, y = {1, 2, 4}
    //   mx = 1, my = 7/3, sxy = (-1)(-4/3) + 0 + (1)(5/3) = 3, sxx = 2 -> b = 1.5
    //   a = 7/3 - 1.5 = 0.8333, syy = 16/9 + 1/9 + 25/9 = 42/9 -> R^2 = 9 / (2 * 42/9) = 0.9643
    const LineFit g = fitLine({0, 1, 2}, {1, 2, 4});
    HEA_CHECK_NEAR(g.slope, 1.5, 1e-12);
    HEA_CHECK_NEAR(g.intercept, 7.0 / 3.0 - 1.5, 1e-12);
    HEA_CHECK_NEAR(g.r2, 81.0 / 84.0, 1e-12);

    // Tüm x aynı: eğim 0, kesişim = ortalama
    const LineFit h = fitLine({1, 1}, {2, 4});
    HEA_CHECK_NEAR(h.intercept, 3.0, 1e-12);
    HEA_CHECK_NEAR(h.slope, 0.0, 0.0);
}

static void testUnitHelpers()
{
    // 0.745 kg = 1 L; 1 L / 10 km = 10 L/100 km
    HEA_CHECK_NEAR(litersPer100km(0.745, 10000.0), 10.0, 1e-12);
    // m_esd = 0.5 kg + 2.8/43e6 * 3.6e6 J (1 kWh) = 0.5 + 0.23442 kg
    HEA_CHECK_NEAR(equivalentFuel_kg(0.5, 3.6e6), 0.5 + 2.8 * 3.6e6 / 43e6, 1e-12);
    // Varsayılan k: 1 kWh = 234.4 g
    HEA_CHECK_NEAR(kDefaultFuelPerBatteryJoule * 3.6e6 * 1000.0, 234.42, 0.01);
}

static void testCorrectionOnChargeSustainingStrategy()
{
    // Kural tabanlı strateji farklı başlangıç SOC'lerinde farklı E_bat üretir;
    // noktalar E_bat = 0'ın iki tarafına düşmeli (dış değerleme yok).
    const DriveCycle c = makeUrbanCycle(101);
    RuleBasedStrategy rules;
    const SocCorrectedFuel r = socCorrectedFuel(SimulationConfig{}, c, rules);
    HEA_CHECK(r.points.size() == 9);
    HEA_CHECK(!r.extrapolated);
    HEA_CHECK(r.fit.r2 > 0.9);              // yakıt ile E_bat arasında güçlü ilişki
    HEA_CHECK(r.localSlope_kgPerJ < 0.0);   // bataryadan çok çeken daha az yakıt harcar
    // Eğim, batarya enerjisinin yakıt karşılığıdır: makul aralıkta olmalı
    // (1 kWh için 100-400 g; 8c'de motorla şarj ~234 g/kWh idi)
    const double gPerKwh = -r.localSlope_kgPerJ * 3.6e6 * 1000.0;
    HEA_CHECK(gPerKwh > 100.0 && gPerKwh < 400.0);
    // Düzeltilmiş yakıt, ham yakıtların aralığında olmalı
    double lo = 1e9, hi = 0.0;
    for (const CorrectionPoint& p : r.points) {
        lo = std::min(lo, p.fuel_kg);
        hi = std::max(hi, p.fuel_kg);
        HEA_CHECK_NEAR(r.distance_m, c.distance_m(), 1e-6);
    }
    HEA_CHECK(r.fuel_kg() >= lo && r.fuel_kg() <= hi);
}

static void testInterpolationByHand()
{
    // Düzeltilmiş değer, E_bat = 0'ı kapsayan iki komşu noktanın ara değeri olmalı.
    // Gerçek bir koşunun noktalarından elle yeniden hesapla.
    const DriveCycle c = makeMixedCycle(121);
    ModelBasedStrategy model;
    const SocCorrectedFuel r = socCorrectedFuel(SimulationConfig{}, c, model);
    HEA_CHECK(!r.extrapolated);
    const CorrectionPoint* neg = nullptr; // E <= 0 olanların en büyüğü
    const CorrectionPoint* pos = nullptr; // E >= 0 olanların en küçüğü
    for (const CorrectionPoint& p : r.points) {
        if (p.batteryEnergy_J <= 0.0 && (neg == nullptr || p.batteryEnergy_J > neg->batteryEnergy_J)) neg = &p;
        if (p.batteryEnergy_J >= 0.0 && (pos == nullptr || p.batteryEnergy_J < pos->batteryEnergy_J)) pos = &p;
    }
    HEA_CHECK(neg != nullptr && pos != nullptr);
    if (neg != nullptr && pos != nullptr) {
        // m = m1 + (m2 - m1) * (0 - E1) / (E2 - E1)
        const double expected = neg->fuel_kg + (pos->fuel_kg - neg->fuel_kg)
                              * (0.0 - neg->batteryEnergy_J)
                              / (pos->batteryEnergy_J - neg->batteryEnergy_J);
        HEA_CHECK_NEAR(r.fuel_kg(), expected, 1e-12);
    }
}

static void testExtrapolationFlag()
{
    // Sadece ICE stratejisi bataryayı tahrikte hiç kullanmaz: tüm koşularda batarya
    // yalnızca rejenle dolar (E_bat < 0), sıfır kapsanmaz -> dış değerleme işaretli.
    const DriveCycle c = makeUrbanCycle(101);
    FixedModeStrategy ice(Mode::Engine);
    const SocCorrectedFuel r = socCorrectedFuel(SimulationConfig{}, c, ice);
    HEA_CHECK(r.extrapolated);
    for (const CorrectionPoint& p : r.points) {
        HEA_CHECK(p.batteryEnergy_J < 0.0);
    }
}

static void testEnergySignMatchesSoc()
{
    // E_bat depolanan enerjidir: işareti her zaman SOC değişimiyle aynı olmalı.
    // (Uç enerjisi kullanılsaydı zorlu_131'de, 0 °C ve yüksek rejen akımında,
    //  SOC düştüğü halde E_bat negatif çıkıyordu.)
    const DriveCycle c = testCycles().back(); // zorlu_131
    ModelBasedStrategy model;
    const SocCorrectedFuel r = socCorrectedFuel(SimulationConfig{}, c, model);
    for (const CorrectionPoint& p : r.points) {
        const double dSoc = p.initialSoc - p.finalSoc;
        HEA_CHECK((p.batteryEnergy_J > 0.0) == (dSoc > 0.0));
    }
}

static void testDeterministic()
{
    const DriveCycle c = makeHighwayCycle(111);
    ModelBasedStrategy a;
    ModelBasedStrategy b;
    HEA_CHECK(socCorrectedFuel(SimulationConfig{}, c, a).fuel_kg()
              == socCorrectedFuel(SimulationConfig{}, c, b).fuel_kg());
}

int main()
{
    testFitLine();
    testUnitHelpers();
    testCorrectionOnChargeSustainingStrategy();
    testInterpolationByHand();
    testExtrapolationFlag();
    testEnergySignMatchesSoc();
    testDeterministic();
    return HEA_TEST_RESULT();
}
