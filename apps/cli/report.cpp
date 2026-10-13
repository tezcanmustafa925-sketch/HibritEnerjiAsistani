#include "report.h"

#include <cmath>
#include <cstdio>

#include "hea/units.h"

namespace cli {

using hea::units::mpsToKmh;
using hea::units::radpsToRpm;

void printStepHeader()
{
    // Birimler başlığın ikinci satırında
    std::printf("%7s %6s %6s %5s %4s %4s %-6s %2s %5s %5s %5s %5s %6s %6s %5s %5s %6s %7s\n",
                "t", "hedef", "hiz", "egim", "gaz", "fren", "mod", "v", "ICE", "ICE",
                "EM", "EM", "bat", "bat", "bat", "SOC", "yakit", "yakit");
    std::printf("%7s %6s %6s %5s %4s %4s %-6s %2s %5s %5s %5s %5s %6s %6s %5s %5s %6s %7s\n",
                "[s]", "[km/h]", "[km/h]", "[%]", "[%]", "[%]", "", "", "[rpm]", "[Nm]",
                "[rpm]", "[Nm]", "[V]", "[A]", "[C]", "[%]", "[g/s]", "top[g]");
}

void printStep(const hea::StepResult& r)
{
    // Öneri uygulanamadıysa mod adının yanına '*' konur (ör. EV istendi, ICE çalıştı)
    char mode[8];
    std::snprintf(mode, sizeof(mode), "%s%s", hea::modeName(r.mode), r.modeOverridden ? "*" : "");
    std::printf("%7.0f %6.1f %6.1f %5.1f %4.0f %4.0f %-6s %2d %5.0f %5.1f %5.0f %5.1f "
                "%6.1f %6.1f %5.1f %5.1f %6.3f %7.1f\n",
                r.time_s, mpsToKmh(r.targetSpeed_mps), mpsToKmh(r.speed_mps),
                std::tan(r.grade_rad) * 100.0, 100.0 * r.throttle, 100.0 * r.brake, mode,
                r.gear, radpsToRpm(r.engineSpeed_radps), r.engineTorque_Nm,
                radpsToRpm(r.motorSpeed_radps), r.motorTorque_Nm, r.batteryVoltage_V,
                r.batteryCurrent_A, r.batteryTemp_C, 100.0 * r.soc,
                1000.0 * r.fuelRate_kgps, 1000.0 * r.fuelUsed_kg);
}

void printSummaryTable(const std::vector<RunRecord>& runs)
{
    std::printf("\nOZET (SOC duzeltmesi yok; SOC farki 10. adimda esdeger yakita cevrilecek)\n");
    std::printf("%-11s %-13s %7s %8s %7s %6s %6s %7s %5s %5s %5s %5s %6s %6s %6s\n",
                "cevrim", "strateji", "yol", "L/100km", "yakit", "SOC0", "SOC1", "dSOC",
                "EV", "ICE", "REG", "DUR", "calis.", "rejen", "uyg.yok");
    std::printf("%-11s %-13s %7s %8s %7s %6s %6s %7s %5s %5s %5s %5s %6s %6s %6s\n",
                "", "", "[km]", "", "[L]", "[%]", "[%]", "[%]", "[%]", "[%]", "[%]", "[%]",
                "[adet]", "[Wh]", "[%]");
    std::string lastCycle;
    for (const RunRecord& run : runs) {
        if (!lastCycle.empty() && run.cycle != lastCycle) {
            std::printf("\n"); // çevrim grupları arasında boş satır
        }
        lastCycle = run.cycle;
        const hea::RunSummary& s = run.summary;
        std::printf("%-11s %-13s %7.2f %8.2f %7.3f %6.1f %6.1f %+7.1f %5.1f %5.1f %5.1f %5.1f "
                    "%6d %6.0f %6.1f\n",
                    run.cycle.c_str(), run.strategy.c_str(), s.distance_m / 1000.0,
                    s.fuelPer100km_L, s.fuel_L, 100.0 * s.socStart, 100.0 * s.socEnd,
                    100.0 * (s.socEnd - s.socStart), 100.0 * s.timeElectric,
                    100.0 * s.timeEngine, 100.0 * s.timeRegen, 100.0 * s.timeStopped,
                    s.engineStarts, s.regenEnergy_J / 3600.0, 100.0 * s.overriddenFraction);
    }
}

} // namespace cli
