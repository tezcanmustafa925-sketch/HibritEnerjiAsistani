#include "csv_export.h"

#include <cstdio>

namespace cli {

bool writeStepsCsv(const std::string& path, const std::vector<RunRecord>& runs)
{
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return false;
    }
    std::fprintf(f,
                 "cycle,strategy,time_s,dt_s,target_speed_mps,speed_mps,acceleration_mps2,"
                 "distance_m,grade_rad,payload_kg,throttle,brake,aero_force_N,"
                 "rolling_force_N,grade_force_N,inertia_force_N,traction_force_N,"
                 "wheel_torque_Nm,wheel_power_W,unmet_wheel_torque_Nm,requested_mode,mode,"
                 "mode_overridden,gear,engine_started,engine_speed_radps,engine_torque_Nm,"
                 "motor_speed_radps,motor_torque_Nm,motor_electrical_power_W,"
                 "friction_brake_power_W,battery_power_W,battery_voltage_V,"
                 "battery_current_A,battery_temp_C,soc,fuel_rate_kgps,fuel_used_kg,"
                 "battery_energy_used_J,engine_running,engine_temp_C,auxiliary_power_W,"
                 "unmet_electrical_power_W\n");
    for (const RunRecord& run : runs) {
        for (const hea::StepResult& r : run.steps) {
            // %.9g: double için yeterli hassasiyet, gereksiz sıfır yok
            std::fprintf(f,
                         "%s,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
                         "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%s,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,"
                         "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%.9g,%.9g,%.9g\n",
                         run.cycle.c_str(), run.strategy.c_str(), r.time_s, r.dt_s,
                         r.targetSpeed_mps, r.speed_mps, r.acceleration_mps2, r.distance_m,
                         r.grade_rad, r.payload_kg, r.throttle, r.brake, r.aeroForce_N,
                         r.rollingForce_N, r.gradeForce_N, r.inertiaForce_N, r.tractionForce_N,
                         r.wheelTorque_Nm, r.wheelPower_W, r.unmetWheelTorque_Nm,
                         hea::modeName(r.requestedMode), hea::modeName(r.mode),
                         r.modeOverridden ? 1 : 0, r.gear, r.engineStarted ? 1 : 0,
                         r.engineSpeed_radps, r.engineTorque_Nm, r.motorSpeed_radps,
                         r.motorTorque_Nm, r.motorElectricalPower_W, r.frictionBrakePower_W,
                         r.batteryPower_W, r.batteryVoltage_V, r.batteryCurrent_A,
                         r.batteryTemp_C, r.soc, r.fuelRate_kgps, r.fuelUsed_kg,
                         r.batteryEnergyUsed_J, r.engineRunning ? 1 : 0, r.engineTemp_C,
                         r.auxiliaryPower_W, r.unmetElectricalPower_W);
        }
    }
    const bool ok = std::ferror(f) == 0;
    return std::fclose(f) == 0 && ok;
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
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return false;
    }
    std::fprintf(f,
                 "cycle,strategy,distance_m,raw_fuel_kg,raw_fuel_per_100km_L,raw_soc_start,"
                 "raw_soc_end,raw_stored_energy_J,corrected_fuel_kg,"
                 "corrected_fuel_per_100km_L,corrected_r2,local_slope_kg_per_J,"
                 "extrapolated,equivalent_fuel_kg,equivalent_fuel_per_100km_L\n");
    for (const ComparisonRow& r : rows) {
        const double eq = hea::equivalentFuel_kg(r.raw.fuel_kg, r.rawStoredEnergy_J);
        std::fprintf(f,
                     "%s,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%.9g,%.9g\n",
                     r.cycle.c_str(), r.strategy.c_str(), r.raw.distance_m, r.raw.fuel_kg,
                     r.raw.fuelPer100km_L, r.raw.socStart, r.raw.socEnd, r.rawStoredEnergy_J,
                     r.corrected.fuel_kg(), r.corrected.fuelPer100km_L(), r.corrected.fit.r2,
                     r.corrected.localSlope_kgPerJ, r.corrected.extrapolated ? 1 : 0, eq,
                     hea::litersPer100km(eq, r.raw.distance_m));
    }
    const bool ok = std::ferror(f) == 0;
    return std::fclose(f) == 0 && ok;
}

} // namespace cli
