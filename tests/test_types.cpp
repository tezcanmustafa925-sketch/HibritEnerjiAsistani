// Temel tipler ve birim dönüşümleri için testler.

#include <cstring>

#include "hea/drive_input.h"
#include "hea/mode.h"
#include "hea/step_result.h"
#include "hea/units.h"
#include "hea/vehicle_params.h"
#include "test_util.h"

using namespace hea;

static void testSpeedConversion()
{
    // 36 km/h = 10 m/s, 100 km/h = 27.777... m/s
    HEA_CHECK_NEAR(units::kmhToMps(36.0), 10.0, 1e-12);
    HEA_CHECK_NEAR(units::kmhToMps(100.0), 27.7777777778, 1e-9);
    HEA_CHECK_NEAR(units::mpsToKmh(units::kmhToMps(123.4)), 123.4, 1e-12);
}

static void testAngularConversion()
{
    // 60 dev/dak = 1 dev/s = 2*pi rad/s
    HEA_CHECK_NEAR(units::rpmToRadps(60.0), 2.0 * constants::kPi, 1e-12);
    HEA_CHECK_NEAR(units::radpsToRpm(units::rpmToRadps(2500.0)), 2500.0, 1e-9);
}

static void testEnergyConversion()
{
    HEA_CHECK_NEAR(units::jouleToKwh(3.6e6), 1.0, 1e-12);
    HEA_CHECK_NEAR(units::kwhToJoule(1.3), 4.68e6, 1e-6);
}

static void testGradeConversion()
{
    HEA_CHECK_NEAR(units::gradePercentToRad(0.0), 0.0, 1e-12);
    // %100 eğim = 45 derece = pi/4 rad
    HEA_CHECK_NEAR(units::gradePercentToRad(100.0), constants::kPi / 4.0, 1e-12);
    // %5 eğim: atan(0.05) = 0.0499583957 rad
    HEA_CHECK_NEAR(units::gradePercentToRad(5.0), 0.0499583957, 1e-9);
    // İniş negatif açı verir
    HEA_CHECK(units::gradePercentToRad(-3.0) < 0.0);
}

static void testModeNames()
{
    HEA_CHECK(std::strcmp(modeName(Mode::Stopped), "DUR") == 0);
    HEA_CHECK(std::strcmp(modeName(Mode::Electric), "EV") == 0);
    HEA_CHECK(std::strcmp(modeName(Mode::Engine), "ICE") == 0);
    HEA_CHECK(std::strcmp(modeName(Mode::RegenBraking), "REGEN") == 0);
}

static void testVehicleParams()
{
    VehicleParams p;
    HEA_CHECK(p.isValid());

    VehicleParams badMass = p;
    badMass.curbMass_kg = 0.0;
    HEA_CHECK(!badMass.isValid());

    VehicleParams badDelta = p;
    badDelta.rotationalMassFactor = 0.9; // dönen kütle toplam kütleyi azaltamaz
    HEA_CHECK(!badDelta.isValid());
}

static void testDefaults()
{
    // Varsayılan oluşturulan yapılar sıfır/güvenli değerlerle başlamalı
    DriveInput in;
    HEA_CHECK_NEAR(in.targetSpeed_mps, 0.0, 0.0);
    HEA_CHECK_NEAR(in.grade_rad, 0.0, 0.0);
    HEA_CHECK(in.payload_kg > 0.0);

    StepResult r;
    HEA_CHECK(r.mode == Mode::Stopped);
    HEA_CHECK_NEAR(r.speed_mps, 0.0, 0.0);
    HEA_CHECK_NEAR(r.fuelUsed_kg, 0.0, 0.0);
}

int main()
{
    testSpeedConversion();
    testAngularConversion();
    testEnergyConversion();
    testGradeConversion();
    testModeNames();
    testVehicleParams();
    testDefaults();
    return HEA_TEST_RESULT();
}
