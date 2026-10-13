#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "hea/drive_cycle.h"
#include "hea/rule_based_strategy.h"
#include "hea/simulation.h"
#include "hea/strategy.h"
#include "hea/electric_motor.h"
#include "hea/engine.h"
#include "hea/units.h"
#include "hea/vehicle_model.h"
#include "hea/version.h"

// Geçici: araç modelinin sabit hızda ürettiği yol yükünü tablo olarak gösterir.
// Simülasyon döngüsü eklendiğinde (5. adım) yerini adım çıktılarına bırakacak.
static void printRoadLoadTable(const hea::VehicleModel& model, double gradePercent)
{
    const double theta = hea::units::gradePercentToRad(gradePercent);
    std::printf("\nSabit hiz yol yuku (egim %%%.0f, yuk 75 kg)\n", gradePercent);
    std::printf("%8s %9s %9s %9s %9s %10s\n",
                "km/h", "aero[N]", "yuv[N]", "egim[N]", "top[N]", "guc[kW]");
    for (int kmh = 0; kmh <= 140; kmh += 20) {
        const double v = hea::units::kmhToMps(kmh);
        const hea::RoadLoad load = model.roadLoad(v, 0.0, theta, 75.0);
        std::printf("%8d %9.1f %9.1f %9.1f %9.1f %10.2f\n", kmh,
                    load.aero_N, load.rolling_N, load.grade_N, load.total_N,
                    hea::VehicleModel::wheelPower(load.total_N, v) / 1000.0);
    }
}

// Geçici: içten yanmalı motorun BSFC haritası [g/kWh]. "-" = tam yük sınırının üstü.
static void printBsfcMap(const hea::Engine& engine)
{
    std::printf("\nICE BSFC haritasi [g/kWh] (satir: tork Nm, sutun: dev/dak)\n%6s", "Nm");
    for (int rpm = 1000; rpm <= 5000; rpm += 1000) {
        std::printf("%7d", rpm);
    }
    std::printf("\n");
    for (int t = 140; t >= 20; t -= 20) {
        std::printf("%6d", t);
        for (int rpm = 1000; rpm <= 5000; rpm += 1000) {
            const double w = hea::units::rpmToRadps(rpm);
            if (!engine.canDeliver(t, w)) {
                std::printf("%7s", "-");
                continue;
            }
            // BSFC [g/kWh] = yakit [g/s] / guc [kW] * 3600
            const double bsfc = engine.fuelRate(t, w) * 1000.0 / (t * w / 1000.0) * 3600.0;
            std::printf("%7.0f", bsfc);
        }
        std::printf("\n");
    }
}

// Geçici: elektrik motorunun motor modundaki verimi [%].
static void printMotorEfficiencyMap(const hea::ElectricMotor& motor)
{
    std::printf("\nEM verimi [%%] (satir: tork Nm, sutun: dev/dak)\n%6s", "Nm");
    for (int rpm = 2000; rpm <= 12000; rpm += 2000) {
        std::printf("%7d", rpm);
    }
    std::printf("\n");
    for (int t = 160; t >= 20; t -= 20) {
        std::printf("%6d", t);
        for (int rpm = 2000; rpm <= 12000; rpm += 2000) {
            const double w = hea::units::rpmToRadps(rpm);
            if (!motor.canDeliver(t, w)) {
                std::printf("%7s", "-");
                continue;
            }
            std::printf("%7.1f", 100.0 * t * w / motor.electricalPower(t, w));
        }
        std::printf("\n");
    }
}

// Çevrim kümesinin özet tablosu (hızlar km/h).
static void printCycleTable(const char* title, const std::vector<hea::DriveCycle>& cycles)
{
    using hea::units::mpsToKmh;
    std::printf("\n%s\n", title);
    std::printf("%-11s %7s %8s %8s %8s %6s %7s %7s %6s %5s\n", "cevrim", "sure[s]",
                "yol[km]", "ort km/h", "max km/h", "dur[%]", "egim+[%]", "egim-[%]",
                "yuk[kg]", "T[C]");
    for (const hea::DriveCycle& c : cycles) {
        double gMax = 0.0;
        double gMin = 0.0;
        for (const hea::DriveInput& s : c.samples) {
            gMax = std::max(gMax, std::tan(s.grade_rad) * 100.0);
            gMin = std::min(gMin, std::tan(s.grade_rad) * 100.0);
        }
        const hea::DriveInput& first = c.samples.front();
        std::printf("%-11s %7.0f %8.2f %8.1f %8.1f %6.1f %7.1f %8.1f %7.0f %5.0f\n",
                    c.name.c_str(), c.duration_s(), c.distance_m() / 1000.0,
                    mpsToKmh(c.averageSpeed_mps()), mpsToKmh(c.maxSpeed_mps()),
                    100.0 * c.stopFraction(), gMax, gMin, first.payload_kg,
                    first.ambientTemp_C);
    }
}

// Geçici (7. adımda ayrıntılı çıktıyla değişecek): her çevrimi verilen stratejiyle
// çalıştırıp tek satırlık özet yazar.
static void printRunTable(const std::vector<hea::DriveCycle>& cycles, hea::Strategy& strategy)
{
    const hea::SimulationConfig cfg;
    std::printf("\nStrateji: %s\n", strategy.name());
    std::printf("%-11s %8s %8s %6s %6s %6s %6s %6s %6s %7s %7s %6s\n", "cevrim", "L/100km",
                "yakit[L]", "SOC0", "SOC1", "EV[%]", "ICE[%]", "REG[%]", "DUR[%]",
                "rejen", "surt.fr", "calis.");
    std::printf("%-11s %8s %8s %6s %6s %6s %6s %6s %6s %7s %7s %6s\n", "", "", "", "", "", "",
                "", "", "", "[Wh]", "[Wh]", "[adet]");
    for (const hea::DriveCycle& c : cycles) {
        const auto steps = hea::runCycle(cfg, c, strategy);
        const hea::RunSummary s = hea::summarize(steps, cfg.initialSoc);
        std::printf("%-11s %8.2f %8.3f %6.3f %6.3f %6.1f %6.1f %6.1f %6.1f %7.0f %7.0f %6d\n",
                    c.name.c_str(), s.fuelPer100km_L, s.fuel_L, s.socStart, s.socEnd,
                    100.0 * s.timeElectric, 100.0 * s.timeEngine, 100.0 * s.timeRegen,
                    100.0 * s.timeStopped, s.regenEnergy_J / 3600.0,
                    s.frictionBrakeEnergy_J / 3600.0, s.engineStarts);
    }
}

int main(int argc, char** argv)
{
    // Ekrana yazma işi yalnızca uygulama katmanında yapılır, core'da değil.
    std::printf("Akilli Hibrit Enerji Asistani (core %s)\n", hea::coreVersion());

    // --tablolar: bileşen modellerinin kontrol tablolarını da göster
    if (argc > 1 && std::strcmp(argv[1], "--tablolar") == 0) {
        const hea::VehicleModel model{hea::VehicleParams{}};
        printRoadLoadTable(model, 0.0);
        printRoadLoadTable(model, 5.0);
        printBsfcMap(hea::Engine{hea::EngineParams{}});
        printMotorEfficiencyMap(hea::ElectricMotor{hea::MotorParams{}});
    }

    printCycleTable("EGITIM CEVRIMLERI", hea::trainingCycles());
    printCycleTable("TEST CEVRIMLERI", hea::testCycles());

    const auto tests = hea::testCycles();
    hea::FixedModeStrategy iceOnly(hea::Mode::Engine);
    hea::FixedModeStrategy evFirst(hea::Mode::Electric);
    hea::RuleBasedStrategy rules;
    printRunTable(tests, iceOnly);
    printRunTable(tests, evFirst);
    printRunTable(tests, rules);
    return 0;
}
