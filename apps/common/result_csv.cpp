#include "result_csv.h"

#include <cstdarg>
#include <cstdio>

#include "hea/mode.h"

namespace hea_csv {

namespace {

// printf biçimli bir satırı akışa yazar (uzunluk sınırı yok: çevrim adı uzun olabilir)
void writef(std::ostream& out, const char* fmt, ...)
{
    char small[1024];
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(small, sizeof(small), fmt, args);
    va_end(args);
    if (n < 0) {
        va_end(copy);
        out.setstate(std::ios::failbit);
        return;
    }
    if (static_cast<std::size_t>(n) < sizeof(small)) {
        out.write(small, n);
    } else {
        std::string big(static_cast<std::size_t>(n) + 1, '\0');
        std::vsnprintf(big.data(), big.size(), fmt, copy);
        out.write(big.data(), n);
    }
    va_end(copy);
}

} // namespace

void writeStepsHeader(std::ostream& out)
{
    out << "cycle,strategy,time_s,dt_s,target_speed_mps,speed_mps,acceleration_mps2,"
           "distance_m,grade_rad,payload_kg,throttle,brake,aero_force_N,"
           "rolling_force_N,grade_force_N,inertia_force_N,traction_force_N,"
           "wheel_torque_Nm,wheel_power_W,unmet_wheel_torque_Nm,requested_mode,mode,"
           "mode_overridden,gear,engine_started,engine_speed_radps,engine_torque_Nm,"
           "motor_speed_radps,motor_torque_Nm,motor_electrical_power_W,"
           "friction_brake_power_W,battery_power_W,battery_voltage_V,"
           "battery_current_A,battery_temp_C,soc,fuel_rate_kgps,fuel_used_kg,"
           "battery_energy_used_J,engine_running,engine_temp_C,auxiliary_power_W,"
           "unmet_electrical_power_W\n";
}

void writeStepRows(std::ostream& out, const std::string& cycle, const std::string& strategy,
                   const std::vector<hea::StepResult>& steps)
{
    for (const hea::StepResult& r : steps) {
        // %.9g: double için yeterli hassasiyet, gereksiz sıfır yok
        writef(out,
               "%s,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
               "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%s,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,"
               "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%.9g,%.9g,%.9g\n",
               cycle.c_str(), strategy.c_str(), r.time_s, r.dt_s, r.targetSpeed_mps, r.speed_mps,
               r.acceleration_mps2, r.distance_m, r.grade_rad, r.payload_kg, r.throttle, r.brake,
               r.aeroForce_N, r.rollingForce_N, r.gradeForce_N, r.inertiaForce_N,
               r.tractionForce_N, r.wheelTorque_Nm, r.wheelPower_W, r.unmetWheelTorque_Nm,
               hea::modeName(r.requestedMode), hea::modeName(r.mode), r.modeOverridden ? 1 : 0,
               r.gear, r.engineStarted ? 1 : 0, r.engineSpeed_radps, r.engineTorque_Nm,
               r.motorSpeed_radps, r.motorTorque_Nm, r.motorElectricalPower_W,
               r.frictionBrakePower_W, r.batteryPower_W, r.batteryVoltage_V, r.batteryCurrent_A,
               r.batteryTemp_C, r.soc, r.fuelRate_kgps, r.fuelUsed_kg, r.batteryEnergyUsed_J,
               r.engineRunning ? 1 : 0, r.engineTemp_C, r.auxiliaryPower_W,
               r.unmetElectricalPower_W);
    }
}

void writeComparisonHeader(std::ostream& out)
{
    out << "cycle,strategy,distance_m,raw_fuel_kg,raw_fuel_per_100km_L,raw_soc_start,"
           "raw_soc_end,raw_stored_energy_J,corrected_fuel_kg,"
           "corrected_fuel_per_100km_L,corrected_r2,local_slope_kg_per_J,"
           "extrapolated,equivalent_fuel_kg,equivalent_fuel_per_100km_L\n";
}

void writeComparisonRow(std::ostream& out, const std::string& cycle, const std::string& strategy,
                        const hea::RunSummary& raw, double rawStoredEnergy_J,
                        const hea::SocCorrectedFuel& corrected)
{
    const double eq = hea::equivalentFuel_kg(raw.fuel_kg, rawStoredEnergy_J);
    writef(out, "%s,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%.9g,%.9g\n",
           cycle.c_str(), strategy.c_str(), raw.distance_m, raw.fuel_kg, raw.fuelPer100km_L,
           raw.socStart, raw.socEnd, rawStoredEnergy_J, corrected.fuel_kg(),
           corrected.fuelPer100km_L(), corrected.fit.r2, corrected.localSlope_kgPerJ,
           corrected.extrapolated ? 1 : 0, eq, hea::litersPer100km(eq, raw.distance_m));
}

} // namespace hea_csv
