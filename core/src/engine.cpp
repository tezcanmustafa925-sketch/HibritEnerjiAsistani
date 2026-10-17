#include "hea/engine.h"

#include <algorithm>
#include <cstddef>

#include "hea/units.h"

namespace hea {

namespace {
// Devir toleransı: kayan nokta yuvarlamasıyla sınırda reddedilmemek için [rad/s]
constexpr double kSpeedTol = 1e-6;
} // namespace

bool EngineParams::isValid() const
{
    if (fullLoadSpeeds_radps.size() < 2
        || fullLoadSpeeds_radps.size() != fullLoadTorques_Nm.size()) {
        return false;
    }
    for (std::size_t i = 1; i < fullLoadSpeeds_radps.size(); ++i) {
        if (fullLoadSpeeds_radps[i] <= fullLoadSpeeds_radps[i - 1]) {
            return false; // devirler artan sırada olmalı
        }
    }
    return idleSpeed_radps > 0.0 && maxSpeed_radps > idleSpeed_radps
        && indicatedEfficiency > 0.0 && indicatedEfficiency < 1.0
        && frictionTorque0_Nm >= 0.0 && frictionTorqueSlope >= 0.0
        && coldFrictionIncrease >= 0.0 && thermalCapacity_JperK > 0.0
        && heatToCoolantFraction >= 0.0 && heatToCoolantFraction <= 1.0
        && heatLoss_WperK >= 0.0;
}

Engine::Engine(const EngineParams& params)
    : params_(params)
{
}

double Engine::maxTorque(double speed_radps) const
{
    if (speed_radps < params_.idleSpeed_radps - kSpeedTol
        || speed_radps > params_.maxSpeed_radps + kSpeedTol) {
        return 0.0;
    }
    const auto& xs = params_.fullLoadSpeeds_radps;
    const auto& ys = params_.fullLoadTorques_Nm;
    if (speed_radps <= xs.front()) {
        return ys.front();
    }
    if (speed_radps >= xs.back()) {
        return ys.back();
    }
    // Doğrusal ara değer
    const auto it = std::upper_bound(xs.begin(), xs.end(), speed_radps);
    const std::size_t i = static_cast<std::size_t>(it - xs.begin());
    const double frac = (speed_radps - xs[i - 1]) / (xs[i] - xs[i - 1]);
    return ys[i - 1] + (ys[i] - ys[i - 1]) * frac;
}

double Engine::coldFrictionFactor(double engineTemp_C) const
{
    // x = clamp((T_sicak - T_m) / (T_sicak - 20), 0, 1.5)
    const double x = std::clamp((kEngineWarmTemp_C - engineTemp_C) / (kEngineWarmTemp_C - 20.0),
                                0.0, 1.5);
    return 1.0 + params_.coldFrictionIncrease * x;
}

double Engine::frictionTorque(double speed_radps, double engineTemp_C) const
{
    // T_sur = (a + b*omega) * soğukluk çarpanı
    return (params_.frictionTorque0_Nm + params_.frictionTorqueSlope * speed_radps)
         * coldFrictionFactor(engineTemp_C);
}

double Engine::fuelPower(double torque_Nm, double speed_radps, double engineTemp_C) const
{
    // P_yakit = (T + T_sur) * omega / e
    const double t = std::max(torque_Nm, 0.0);
    return (t + frictionTorque(speed_radps, engineTemp_C)) * speed_radps
         / params_.indicatedEfficiency;
}

double Engine::fuelRate(double torque_Nm, double speed_radps, double engineTemp_C) const
{
    // m_dot = P_yakit / LHV
    return fuelPower(torque_Nm, speed_radps, engineTemp_C) / constants::kGasolineLhv;
}

double Engine::efficiency(double torque_Nm, double speed_radps, double engineTemp_C) const
{
    const double pf = fuelPower(torque_Nm, speed_radps, engineTemp_C);
    return pf > 0.0 ? std::max(torque_Nm, 0.0) * speed_radps / pf : 0.0;
}

double Engine::nextTemperature(double engineTemp_C, double fuelPower_W, double ambientTemp_C,
                               double dt_s) const
{
    // C * dT/dt = f * P_yakit - hA * (T - T_ortam)  (açık Euler), termostat: T <= T_sicak
    const double dTdt = (params_.heatToCoolantFraction * fuelPower_W
                         - params_.heatLoss_WperK * (engineTemp_C - ambientTemp_C))
                      / params_.thermalCapacity_JperK;
    return std::min(engineTemp_C + dTdt * dt_s, std::max(kEngineWarmTemp_C, ambientTemp_C));
}

bool Engine::canDeliver(double torque_Nm, double speed_radps) const
{
    return torque_Nm >= 0.0 && torque_Nm <= maxTorque(speed_radps) + 1e-9
        && speed_radps >= params_.idleSpeed_radps - kSpeedTol
        && speed_radps <= params_.maxSpeed_radps + kSpeedTol;
}

} // namespace hea
