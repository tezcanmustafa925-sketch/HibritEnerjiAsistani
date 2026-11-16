#include "csv_export.h"

#include <cstdio>
#include <fstream>

#include "result_csv.h"

namespace cli {

// Adım ve karşılaştırma CSV'lerinin satırları ortak koddan (apps/common/result_csv):
// arayüzün "CSV olarak kaydet"i de aynı biçimi yazar
bool writeStepsCsv(const std::string& path, const std::vector<RunRecord>& runs)
{
    std::ofstream f(path); // metin kipi: Windows'ta satır sonu CRLF (önceki fopen "w" gibi)
    if (!f) {
        return false;
    }
    hea_csv::writeStepsHeader(f);
    for (const RunRecord& run : runs) {
        hea_csv::writeStepRows(f, run.cycle, run.strategy, run.steps);
    }
    f.close();
    return !f.fail();
}

bool writeTrainingCsv(const std::string& path, const std::vector<hea::TrainingSample>& samples)
{
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return false;
    }
    std::fprintf(f,
                 "cycle,time_s,dt_s,speed_mps,acceleration_mps2,grade_rad,payload_kg,"
                 "wheel_torque_Nm,wheel_speed_radps,wheel_power_W,soc,battery_temp_C,"
                 "engine_temp_C,"
                 "battery_max_discharge_W,battery_max_charge_W,"
                 "ev_feasible,ev_battery_power_W,"
                 "ice_feasible,ice_fuel_rate_kgps,ice_gear,"
                 "ice_charge_power_W,ice_charge_fuel_rate_kgps\n");
    for (const hea::TrainingSample& s : samples) {
        const hea::StrategyInput& in = s.in;
        std::fprintf(f,
                     "%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
                     "%d,%.9g,%d,%.9g,%d,%.9g,%.9g\n",
                     s.cycle.c_str(), in.time_s, s.dt_s, in.speed_mps, in.acceleration_mps2,
                     in.grade_rad, in.payload_kg, in.wheelTorque_Nm, in.wheelSpeed_radps,
                     in.wheelPower_W, in.soc, in.batteryTemp_C, in.engineTemp_C,
                     in.batteryMaxDischarge_W,
                     in.batteryMaxCharge_W, s.evFeasible ? 1 : 0, s.evBatteryPower_W,
                     s.iceFeasible ? 1 : 0, s.iceFuelRate_kgps, s.iceGear,
                     s.iceChargePower_W, s.iceChargeFuelRate_kgps);
    }
    const bool ok = std::ferror(f) == 0;
    return std::fclose(f) == 0 && ok;
}

bool writeSummaryCsv(const std::string& path, const std::vector<RunRecord>& runs)
{
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return false;
    }
    std::fprintf(f,
                 "cycle,strategy,duration_s,distance_m,fuel_kg,fuel_L,fuel_per_100km_L,"
                 "soc_start,soc_end,battery_energy_used_J,regen_energy_J,"
                 "friction_brake_energy_J,auxiliary_energy_J,unmet_electrical_energy_J,"
                 "time_electric,time_engine,time_regen,time_stopped,overridden_fraction,"
                 "engine_starts,max_speed_error_mps\n");
    for (const RunRecord& run : runs) {
        const hea::RunSummary& s = run.summary;
        std::fprintf(f,
                     "%s,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
                     "%.9g,%.9g,%.9g,%.9g,%.9g,%d,%.9g\n",
                     run.cycle.c_str(), run.strategy.c_str(), s.duration_s, s.distance_m,
                     s.fuel_kg, s.fuel_L, s.fuelPer100km_L, s.socStart, s.socEnd,
                     s.batteryEnergyUsed_J, s.regenEnergy_J, s.frictionBrakeEnergy_J,
                     s.auxiliaryEnergy_J, s.unmetElectricalEnergy_J, s.timeElectric,
                     s.timeEngine, s.timeRegen, s.timeStopped, s.overriddenFraction,
                     s.engineStarts, s.maxSpeedError_mps);
    }
    const bool ok = std::ferror(f) == 0;
    return std::fclose(f) == 0 && ok;
}

bool writeComparisonCsv(const std::string& path, const std::vector<ComparisonRow>& rows)
{
    std::ofstream f(path);
    if (!f) {
        return false;
    }
    hea_csv::writeComparisonHeader(f);
    for (const ComparisonRow& r : rows) {
        hea_csv::writeComparisonRow(f, r.cycle, r.strategy, r.raw, r.rawStoredEnergy_J,
                                    r.corrected);
    }
    f.close();
    return !f.fail();
}

} // namespace cli
