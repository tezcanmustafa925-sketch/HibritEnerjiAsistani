#pragma once

#include <vector>

#include "hea/consumption_model_coeffs.h"
#include "hea/strategy.h"

namespace hea {

// Python'da eğitilen tek bir lineer model: tahmin = kesişim + sum(katsayı_i * özellik_i).
// Özellikler, stratejinin bildiği tekerlek büyüklüklerinden hesaplanır
// (P: güç [W], T: tork [Nm], w: açısal hız [rad/s]).
class LinearModel {
public:
    LinearModel() = default;
    // Özellik adları tanınmazsa model geçersiz olur (valid() == false).
    explicit LinearModel(const generated::LinearModelCoeffs& coeffs);

    bool valid() const { return valid_; }
    double predict(double power_W, double torque_Nm, double speed_radps) const;

private:
    enum class Feature { P, T, W, T2, W2, W3, TW };
    std::vector<Feature> features_;
    std::vector<double> coefficients_;
    double intercept_ = 0.0;
    bool valid_ = false;
};

// Tüketim tahmin modeli: Python'da eğitilip consumption_model_coeffs.h dosyasına
// aktarılan üç lineer modeli bir arada tutar (bkz. model/egit.py).
class ConsumptionModel {
public:
    // Üretilmiş katsayılarla (consumption_model_coeffs.h) model oluşturur.
    static ConsumptionModel fromGeneratedCoefficients();

    bool valid() const { return ev_.valid() && ice_.valid() && charge_.valid(); }

    // EV seçilseydi bataryadan çekilecek motor gücü [W]
    double evBatteryPower_W(const StrategyInput& in) const;
    // ICE seçilseydi (şarjsız) yakıt debisi [kg/s], >= 0
    double iceFuelRate_kgps(const StrategyInput& in) const;
    // ICE modunda bataryaya verilen her joule için ek yakıt [kg/J], >= 0
    double chargeFuelPerJoule(const StrategyInput& in) const;

private:
    LinearModel ev_;
    LinearModel ice_;
    LinearModel charge_;
};

} // namespace hea
