#include "hea/electric_motor.h"

#include <algorithm>
#include <cmath>

namespace hea {

bool MotorParams::isValid() const
{
    return maxPower_W > 0.0 && maxTorque_Nm > 0.0 && maxSpeed_radps > 0.0
        && copperLossCoeff > 0.0 && ironLossCoeff >= 0.0
        && windageLossCoeff >= 0.0 && constantLoss_W >= 0.0;
}

ElectricMotor::ElectricMotor(const MotorParams& params)
    : params_(params)
{
}

double ElectricMotor::maxTorque(double speed_radps) const
{
    const double w = std::fabs(speed_radps);
    if (w > params_.maxSpeed_radps) {
        return 0.0;
    }
    if (w < 1e-9) {
        return params_.maxTorque_Nm;
    }
    // T_max = min(T_tepe, P_max / omega)
    return std::min(params_.maxTorque_Nm, params_.maxPower_W / w);
}

double ElectricMotor::loss(double torque_Nm, double speed_radps) const
{
    const double w = std::fabs(speed_radps);
    // P_kayip = k_c*T^2 + k_i*omega + k_w*omega^3 + C
    return params_.copperLossCoeff * torque_Nm * torque_Nm
         + params_.ironLossCoeff * w
         + params_.windageLossCoeff * w * w * w
         + params_.constantLoss_W;
}

double ElectricMotor::electricalPower(double torque_Nm, double speed_radps) const
{
    // P_e = T*omega + P_kayip
    return torque_Nm * speed_radps + loss(torque_Nm, speed_radps);
}

double ElectricMotor::torqueForElectricalPower(double power_W, double speed_radps) const
{
    const double w = std::fabs(speed_radps);
    const double kc = params_.copperLossCoeff;
    // Torka bağlı olmayan kayıplar
    const double l0 = params_.ironLossCoeff * w + params_.windageLossCoeff * w * w * w
                    + params_.constantLoss_W;
    // k_c*T^2 + w*T + (l0 - P_e) = 0  ->  T = (-w + sqrt(w^2 - 4*k_c*(l0 - P_e))) / (2*k_c)
    const double disc = w * w - 4.0 * kc * (l0 - power_W);
    if (disc < 0.0) {
        return -w / (2.0 * kc); // en büyük jeneratör gücünün torku
    }
    return (-w + std::sqrt(disc)) / (2.0 * kc);
}

bool ElectricMotor::canDeliver(double torque_Nm, double speed_radps) const
{
    return std::fabs(speed_radps) <= params_.maxSpeed_radps
        && std::fabs(torque_Nm) <= maxTorque(speed_radps) + 1e-9;
}

} // namespace hea
