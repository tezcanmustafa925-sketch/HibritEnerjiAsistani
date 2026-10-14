#include "hea/consumption_model.h"

#include <algorithm>
#include <cstring>

namespace hea {

LinearModel::LinearModel(const generated::LinearModelCoeffs& coeffs)
    : intercept_(coeffs.intercept)
    , valid_(true)
{
    for (std::size_t i = 0; i < coeffs.count; ++i) {
        const char* f = coeffs.features[i];
        if (std::strcmp(f, "P") == 0)        features_.push_back(Feature::P);
        else if (std::strcmp(f, "T") == 0)   features_.push_back(Feature::T);
        else if (std::strcmp(f, "w") == 0)   features_.push_back(Feature::W);
        else if (std::strcmp(f, "T^2") == 0) features_.push_back(Feature::T2);
        else if (std::strcmp(f, "w^2") == 0) features_.push_back(Feature::W2);
        else if (std::strcmp(f, "w^3") == 0) features_.push_back(Feature::W3);
        else if (std::strcmp(f, "T*w") == 0) features_.push_back(Feature::TW);
        else {
            valid_ = false; // bilinmeyen özellik: egit.py ile bu dosya uyumsuz
            continue;
        }
        coefficients_.push_back(coeffs.coefficients[i]);
    }
}

double LinearModel::predict(double power_W, double torque_Nm, double speed_radps) const
{
    double y = intercept_;
    for (std::size_t i = 0; i < features_.size(); ++i) {
        double x = 0.0;
        switch (features_[i]) {
        case Feature::P:  x = power_W; break;
        case Feature::T:  x = torque_Nm; break;
        case Feature::W:  x = speed_radps; break;
        case Feature::T2: x = torque_Nm * torque_Nm; break;
        case Feature::W2: x = speed_radps * speed_radps; break;
        case Feature::W3: x = speed_radps * speed_radps * speed_radps; break;
        case Feature::TW: x = torque_Nm * speed_radps; break;
        }
        y += coefficients_[i] * x;
    }
    return y;
}

ConsumptionModel ConsumptionModel::fromGeneratedCoefficients()
{
    ConsumptionModel m;
    m.ev_ = LinearModel(generated::kEvBatteryPower);
    m.ice_ = LinearModel(generated::kIceFuelRate);
    m.charge_ = LinearModel(generated::kChargeFuelPerJoule);
    return m;
}

double ConsumptionModel::evBatteryPower_W(const StrategyInput& in) const
{
    return ev_.predict(in.wheelPower_W, in.wheelTorque_Nm, in.wheelSpeed_radps);
}

double ConsumptionModel::iceFuelRate_kgps(const StrategyInput& in) const
{
    // Lineer model eğitim aralığı dışında negatif verebilir; yakıt negatif olamaz
    return std::max(0.0, ice_.predict(in.wheelPower_W, in.wheelTorque_Nm, in.wheelSpeed_radps));
}

double ConsumptionModel::chargeFuelPerJoule(const StrategyInput& in) const
{
    return std::max(0.0, charge_.predict(in.wheelPower_W, in.wheelTorque_Nm, in.wheelSpeed_radps));
}

} // namespace hea
