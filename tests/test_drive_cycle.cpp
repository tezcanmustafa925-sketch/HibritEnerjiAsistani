// Sürüş çevrimi oluşturucu ve üreteç testleri.

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "hea/drive_cycle.h"
#include "hea/units.h"
#include "test_util.h"

using namespace hea;

static void testEmptyAndHold()
{
    const DriveCycle empty = CycleBuilder("bos").build();
    HEA_CHECK(empty.samples.size() == 1); // yalnızca t = 0 örneği
    HEA_CHECK_NEAR(empty.duration_s(), 0.0, 0.0);

    const DriveCycle c = CycleBuilder("bekle").hold(5.0).build();
    HEA_CHECK(c.samples.size() == 6);
    HEA_CHECK_NEAR(c.duration_s(), 5.0, 1e-12);
    HEA_CHECK_NEAR(c.distance_m(), 0.0, 0.0);
    HEA_CHECK_NEAR(c.stopFraction(), 1.0, 1e-12);
}

static void testRamp()
{
    // 0 -> 10 m/s, 1 m/s^2: 10 adım, her adımda +1 m/s
    const DriveCycle a = CycleBuilder("ramp").rampTo(10.0, 1.0).build();
    HEA_CHECK(a.samples.size() == 11);
    for (std::size_t i = 0; i < a.samples.size(); ++i) {
        HEA_CHECK_NEAR(a.samples[i].targetSpeed_mps, static_cast<double>(i), 1e-12);
        HEA_CHECK_NEAR(a.samples[i].time_s, static_cast<double>(i), 1e-12);
    }
    HEA_CHECK_NEAR(a.maxAcceleration_mps2(), 1.0, 1e-12);

    // 0 -> 10 m/s, 3 m/s^2: ceil(10/3) = 4 adım, gerçek ivme 2.5 m/s^2 (sınırı aşmaz)
    const DriveCycle b = CycleBuilder("ramp3").rampTo(10.0, 3.0).build();
    HEA_CHECK(b.samples.size() == 5);
    HEA_CHECK_NEAR(b.maxAcceleration_mps2(), 2.5, 1e-12);

    // Negatif hedef hız 0'a kırpılır (araç geri gitmez)
    const DriveCycle c = CycleBuilder("geri").rampTo(5.0, 1.0).rampTo(-3.0, 1.0).build();
    HEA_CHECK_NEAR(c.samples.back().targetSpeed_mps, 0.0, 0.0);
}

static void testDistanceAndStats()
{
    // 0->10 m/s 10 s'de (50 m) + 10 s sabit 10 m/s (100 m) + 10->0 5 s'de (25 m) = 175 m
    const DriveCycle c = CycleBuilder("yol").rampTo(10.0, 1.0).hold(10.0).rampTo(0.0, 2.0).build();
    HEA_CHECK_NEAR(c.duration_s(), 25.0, 1e-12);
    HEA_CHECK_NEAR(c.distance_m(), 175.0, 1e-9);
    HEA_CHECK_NEAR(c.averageSpeed_mps(), 7.0, 1e-12);
    HEA_CHECK_NEAR(c.maxSpeed_mps(), 10.0, 1e-12);
    HEA_CHECK_NEAR(c.maxDeceleration_mps2(), 2.0, 1e-12);
}

static void testGradeProfile()
{
    const std::vector<GradePoint> p = {{0.0, 0.0}, {100.0, 4.0}};
    HEA_CHECK_NEAR(gradeAtDistance({}, 50.0), 0.0, 0.0);
    HEA_CHECK_NEAR(gradeAtDistance(p, -10.0), 0.0, 1e-12);
    // 50 m: %0 ile %4 arasının ortası = %2 -> atan(0.02)
    HEA_CHECK_NEAR(gradeAtDistance(p, 50.0), std::atan(0.02), 1e-12);
    // Son noktadan sonra sabit
    HEA_CHECK_NEAR(gradeAtDistance(p, 500.0), std::atan(0.04), 1e-12);

    // Çevrimde eğim, örneğin konumundaki mesafeden okunur.
    // 10 m/s sabit hızda t = 5 s'de mesafe = 50 m (ilk 1 s'de 0->10 rampası: 5 m, sonra 4*10)
    const DriveCycle c = CycleBuilder("egim").rampTo(10.0, 10.0).hold(10.0).grade(p).build();
    HEA_CHECK_NEAR(c.samples[0].grade_rad, 0.0, 1e-12);
    // t = 5 s: 5 + 4*10 = 45 m -> %1.8
    HEA_CHECK_NEAR(c.samples[5].grade_rad, units::gradePercentToRad(1.8), 1e-12);
}

static void testPayloadAndAmbient()
{
    const DriveCycle c = CycleBuilder("yuk").hold(2.0).payload(320.0).ambient(-5.0).build();
    for (const DriveInput& s : c.samples) {
        HEA_CHECK_NEAR(s.payload_kg, 320.0, 0.0);
        HEA_CHECK_NEAR(s.ambientTemp_C, -5.0, 0.0);
    }
}

static bool sameSpeeds(const DriveCycle& a, const DriveCycle& b)
{
    if (a.samples.size() != b.samples.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.samples.size(); ++i) {
        if (a.samples[i].targetSpeed_mps != b.samples[i].targetSpeed_mps
            || a.samples[i].grade_rad != b.samples[i].grade_rad) {
            return false;
        }
    }
    return true;
}

static void checkCommon(const DriveCycle& c)
{
    // Durarak başlar ve biter, zaman ekseni düzgün
    HEA_CHECK_NEAR(c.samples.front().targetSpeed_mps, 0.0, 0.0);
    HEA_CHECK_NEAR(c.samples.back().targetSpeed_mps, 0.0, 1e-12);
    for (std::size_t i = 0; i < c.samples.size(); ++i) {
        HEA_CHECK_NEAR(c.samples[i].time_s, static_cast<double>(i) * c.dt_s, 1e-9);
        HEA_CHECK(c.samples[i].targetSpeed_mps >= 0.0);
    }
    // Binek araç için makul ivmeler
    HEA_CHECK(c.maxAcceleration_mps2() <= 1.5 + 1e-9);
    HEA_CHECK(c.maxDeceleration_mps2() <= 1.6 + 1e-9);
}

static void testGenerators()
{
    // Aynı tohum -> aynı çevrim; farklı tohum -> farklı çevrim
    HEA_CHECK(sameSpeeds(makeUrbanCycle(7), makeUrbanCycle(7)));
    HEA_CHECK(!sameSpeeds(makeUrbanCycle(7), makeUrbanCycle(8)));

    const DriveCycle u = makeUrbanCycle(1);
    checkCommon(u);
    HEA_CHECK(u.duration_s() >= 1200.0);
    HEA_CHECK(u.maxSpeed_mps() <= units::kmhToMps(55.0) + 1e-9);
    HEA_CHECK(u.stopFraction() > 0.1); // şehirde önemli bekleme süresi

    const DriveCycle h = makeHighwayCycle(11);
    checkCommon(h);
    HEA_CHECK(h.maxSpeed_mps() <= units::kmhToMps(130.0) + 1e-9);
    HEA_CHECK(h.averageSpeed_mps() > units::kmhToMps(80.0));
    HEA_CHECK(h.stopFraction() < 0.05);

    const DriveCycle m = makeMixedCycle(21);
    checkCommon(m);
    HEA_CHECK(m.duration_s() >= 1800.0);
    for (const DriveInput& s : m.samples) {
        HEA_CHECK(std::fabs(s.grade_rad) <= units::gradePercentToRad(5.0) + 1e-12);
    }
}

static void testTrainTestSplit()
{
    const auto train = trainingCycles();
    const auto test = testCycles();
    HEA_CHECK(train.size() == 7);
    HEA_CHECK(test.size() == 4);

    // Adlar benzersiz ve iki küme ayrık olmalı
    std::set<std::string> names;
    for (const auto& c : train) {
        HEA_CHECK(names.insert(c.name).second);
    }
    for (const auto& c : test) {
        HEA_CHECK(names.insert(c.name).second);
        // Test çevrimi hiçbir eğitim çevrimiyle aynı olmamalı
        for (const auto& t : train) {
            HEA_CHECK(!sameSpeeds(c, t));
        }
    }

    // Zorlu test çevrimi: 400 kg, 0 °C, eğim %7'ye kadar
    const DriveCycle& hard = test.back();
    HEA_CHECK(hard.name == "zorlu_131");
    HEA_CHECK_NEAR(hard.samples[10].payload_kg, 400.0, 0.0);
    HEA_CHECK_NEAR(hard.samples[10].ambientTemp_C, 0.0, 0.0);
    double maxGrade = 0.0;
    for (const DriveInput& s : hard.samples) {
        maxGrade = std::max(maxGrade, std::fabs(s.grade_rad));
    }
    HEA_CHECK(maxGrade > units::gradePercentToRad(5.0)); // eğitimdekinden dik
    HEA_CHECK(maxGrade <= units::gradePercentToRad(7.0) + 1e-12);
}

int main()
{
    testEmptyAndHold();
    testRamp();
    testDistanceAndStats();
    testGradeProfile();
    testPayloadAndAmbient();
    testGenerators();
    testTrainTestSplit();
    return HEA_TEST_RESULT();
}
