#include "hea/vehicle_model.h"

#include <cmath>

#include "hea/units.h"

namespace hea {

namespace {
// Bu hızın altında araç "duruyor" kabul edilir [m/s]
constexpr double kStandstillSpeed_mps = 1e-3;
} // namespace

VehicleModel::VehicleModel(const VehicleParams& params)
    : params_(params)
{
}

double VehicleModel::totalMass(double payload_kg) const
{
    return params_.curbMass_kg + payload_kg;
}

double VehicleModel::aeroForce(double speed_mps) const
{
    // F_aero = 1/2 * rho * Cd * A * v^2
    return 0.5 * constants::kAirDensity * params_.dragCoefficient
         * params_.frontalArea_m2 * speed_mps * speed_mps;
}

double VehicleModel::rollingForce(double speed_mps, double mass_kg, double grade_rad) const
{
    if (speed_mps < kStandstillSpeed_mps) {
        return 0.0; // duran araçta yuvarlanma direnci harekete karşı koymaz
    }
    // F_yuv = Crr * m * g * cos(theta)
    return params_.rollingResistanceCoeff * mass_kg * constants::kGravity
         * std::cos(grade_rad);
}

double VehicleModel::gradeForce(double mass_kg, double grade_rad) const
{
    // F_egim = m * g * sin(theta)
    return mass_kg * constants::kGravity * std::sin(grade_rad);
}

double VehicleModel::inertiaForce(double mass_kg, double accel_mps2) const
{
    // F_atalet = delta * m * a
    return params_.rotationalMassFactor * mass_kg * accel_mps2;
}

RoadLoad VehicleModel::roadLoad(double speed_mps, double accel_mps2,
                                double grade_rad, double payload_kg) const
{
    const double m = totalMass(payload_kg);

    RoadLoad load;
    load.aero_N = aeroForce(speed_mps);
    load.rolling_N = rollingForce(speed_mps, m, grade_rad);
    load.grade_N = gradeForce(m, grade_rad);
    load.inertia_N = inertiaForce(m, accel_mps2);
    // F_cekis = F_aero + F_yuv + F_egim + F_atalet
    load.total_N = load.aero_N + load.rolling_N + load.grade_N + load.inertia_N;
    return load;
}

double VehicleModel::wheelTorque(double force_N) const
{
    // T = F * r
    return force_N * params_.wheelRadius_m;
}

double VehicleModel::wheelPower(double force_N, double speed_mps)
{
    // P = F * v
    return force_N * speed_mps;
}

double VehicleModel::wheelSpeed(double speed_mps) const
{
    // omega = v / r
    return speed_mps / params_.wheelRadius_m;
}

} // namespace hea
