// Araç modeli testleri. Beklenen değerler varsayılan parametrelerle elle hesaplandı:
//   m   = 1380 kg (boş) + 75 kg (yük) = 1455 kg
//   m*g = 1455 * 9.81 = 14273.55 N
//   1/2 * rho * Cd * A = 0.5 * 1.225 * 0.24 * 2.2 = 0.3234 kg/m

#include <cmath>

#include "hea/units.h"
#include "hea/vehicle_model.h"
#include "test_util.h"

using namespace hea;

static const VehicleModel kModel{VehicleParams{}};
static constexpr double kPayload = 75.0;
static constexpr double kMass = 1455.0;

static void testTotalMass()
{
    HEA_CHECK_NEAR(kModel.totalMass(kPayload), kMass, 1e-12);
    HEA_CHECK_NEAR(kModel.totalMass(400.0), 1780.0, 1e-12);
}

static void testAeroForce()
{
    // 100 km/h = 27.7778 m/s -> v^2 = 771.6049 -> 0.3234 * 771.6049 = 249.537 N
    HEA_CHECK_NEAR(kModel.aeroForce(units::kmhToMps(100.0)), 249.537037, 1e-5);
    // Hız yarıya inince kuvvet dörtte bire iner (v^2 bağımlılığı)
    HEA_CHECK_NEAR(kModel.aeroForce(units::kmhToMps(50.0)), 249.537037 / 4.0, 1e-5);
    HEA_CHECK_NEAR(kModel.aeroForce(0.0), 0.0, 1e-12);
}

static void testRollingForce()
{
    // Düz yol: 0.009 * 14273.55 = 128.46195 N, hızdan bağımsız
    HEA_CHECK_NEAR(kModel.rollingForce(1.0, kMass, 0.0), 128.46195, 1e-6);
    HEA_CHECK_NEAR(kModel.rollingForce(30.0, kMass, 0.0), 128.46195, 1e-6);
    // Duran araçta sıfır
    HEA_CHECK_NEAR(kModel.rollingForce(0.0, kMass, 0.0), 0.0, 1e-12);
    // %5 eğim: cos(atan(0.05)) = 0.99875234 -> 128.30167 N
    const double theta = units::gradePercentToRad(5.0);
    HEA_CHECK_NEAR(kModel.rollingForce(10.0, kMass, theta), 128.30167, 1e-4);
}

static void testGradeForce()
{
    HEA_CHECK_NEAR(kModel.gradeForce(kMass, 0.0), 0.0, 1e-12);
    // %5 yokuş: sin(atan(0.05)) = 0.04993762 -> 14273.55 * 0.04993762 = 712.787 N
    const double up = units::gradePercentToRad(5.0);
    HEA_CHECK_NEAR(kModel.gradeForce(kMass, up), 712.787, 1e-3);
    // Aynı eğimde iniş: eşit büyüklükte, ters işaretli
    HEA_CHECK_NEAR(kModel.gradeForce(kMass, -up), -712.787, 1e-3);
}

static void testInertiaForce()
{
    // 1.05 * 1455 * 1.0 = 1527.75 N
    HEA_CHECK_NEAR(kModel.inertiaForce(kMass, 1.0), 1527.75, 1e-9);
    // Yavaşlamada negatif
    HEA_CHECK_NEAR(kModel.inertiaForce(kMass, -2.0), -3055.5, 1e-9);
}

static void testCruise100Kmh()
{
    // 100 km/h sabit hız, düz yol: F = 249.537 + 128.462 = 377.999 N
    // P = 377.999 * 27.7778 = 10500 W (~10.5 kW; kompakt hibrit için makul)
    const double v = units::kmhToMps(100.0);
    const RoadLoad load = kModel.roadLoad(v, 0.0, 0.0, kPayload);
    HEA_CHECK_NEAR(load.inertia_N, 0.0, 1e-12);
    HEA_CHECK_NEAR(load.grade_N, 0.0, 1e-12);
    HEA_CHECK_NEAR(load.total_N, 377.998987, 1e-5);
    HEA_CHECK_NEAR(VehicleModel::wheelPower(load.total_N, v), 10499.97, 1e-2);
    // Toplam, bileşenlerin toplamına eşit olmalı
    HEA_CHECK_NEAR(load.total_N,
                   load.aero_N + load.rolling_N + load.grade_N + load.inertia_N, 1e-9);
}

static void testCoastDown()
{
    // Serbest yuvarlanma: tekerlekte kuvvet yokken araç a = -(F_aero + F_yuv) / (delta*m)
    // ile yavaşlar. Bu ivmeyi verince toplam çekiş kuvveti sıfır çıkmalı.
    const double v = units::kmhToMps(100.0);
    const double a = -(kModel.aeroForce(v) + kModel.rollingForce(v, kMass, 0.0))
                   / (1.05 * kMass);
    HEA_CHECK_NEAR(a, -0.247422, 1e-5);
    HEA_CHECK_NEAR(kModel.roadLoad(v, a, 0.0, kPayload).total_N, 0.0, 1e-9);
}

static void testBrakingGivesNegativePower()
{
    // 50 km/h'de -3 m/s^2 frenleme: atalet kuvveti dirençlerden büyük -> negatif güç
    const double v = units::kmhToMps(50.0);
    const RoadLoad load = kModel.roadLoad(v, -3.0, 0.0, kPayload);
    HEA_CHECK(load.total_N < 0.0);
    HEA_CHECK(VehicleModel::wheelPower(load.total_N, v) < 0.0);
}

static void testDownhillCoastPushesCar()
{
    // %6 inişte 50 km/h sabit hız: eğim kuvveti dirençleri aşar -> fren gerekir
    const RoadLoad load = kModel.roadLoad(units::kmhToMps(50.0), 0.0,
                                          units::gradePercentToRad(-6.0), kPayload);
    HEA_CHECK(load.total_N < 0.0);
}

static void testPayloadEffect()
{
    // Yük arttıkça yuvarlanma ve eğim kuvveti kütleyle orantılı artar, aero değişmez
    const double v = 15.0;
    const double theta = units::gradePercentToRad(3.0);
    const RoadLoad light = kModel.roadLoad(v, 0.0, theta, 75.0);
    const RoadLoad heavy = kModel.roadLoad(v, 0.0, theta, 475.0);
    HEA_CHECK_NEAR(heavy.aero_N, light.aero_N, 1e-12);
    HEA_CHECK_NEAR(heavy.rolling_N / light.rolling_N, 1855.0 / 1455.0, 1e-12);
    HEA_CHECK_NEAR(heavy.grade_N / light.grade_N, 1855.0 / 1455.0, 1e-12);
}

static void testWheelKinematics()
{
    // T = F * r = 1000 * 0.30 = 300 Nm
    HEA_CHECK_NEAR(kModel.wheelTorque(1000.0), 300.0, 1e-12);
    // P = F * v = 1000 * 20 = 20 kW
    HEA_CHECK_NEAR(VehicleModel::wheelPower(1000.0, 20.0), 20000.0, 1e-9);
    // omega = v / r = 30 / 0.30 = 100 rad/s
    HEA_CHECK_NEAR(kModel.wheelSpeed(30.0), 100.0, 1e-12);
}

int main()
{
    testTotalMass();
    testAeroForce();
    testRollingForce();
    testGradeForce();
    testInertiaForce();
    testCruise100Kmh();
    testCoastDown();
    testBrakingGivesNegativePower();
    testDownhillCoastPushesCar();
    testPayloadEffect();
    testWheelKinematics();
    return HEA_TEST_RESULT();
}
