#include "hea/battery.h"

#include <algorithm>
#include <cmath>

namespace hea {

namespace {

// Tipik NMC Li-iyon hücre OCV eğrisi: SOC = 0.0, 0.1, ..., 1.0 noktalarında gerilim [V].
// Uçlarda dik, ortada (0.3-0.7) yatık bir eğri; hibritler bu yatık bölgede çalışır.
constexpr double kCellOcvTable[] = {
    3.00, 3.45, 3.55, 3.62, 3.67, 3.72, 3.78, 3.86, 3.95, 4.05, 4.18};
constexpr int kOcvPoints = sizeof(kCellOcvTable) / sizeof(kCellOcvTable[0]);

// Akım için ikinci derece denklemin çözümü. P: uç gücü (deşarj pozitif).
// I = (OCV - sqrt(OCV^2 - 4*R*P)) / (2*R)
double currentForPower(double ocv, double r, double power)
{
    const double disc = ocv * ocv - 4.0 * r * power;
    // disc < 0: bu güç hiçbir akımla çekilemez. Çağıran taraf gücü önceden
    // sınırladığı için olmamalı; yine de tepe noktası I = OCV / (2R) döndürülür.
    if (disc < 0.0) {
        return ocv / (2.0 * r);
    }
    return (ocv - std::sqrt(disc)) / (2.0 * r);
}

} // namespace

bool BatteryParams::isValid() const
{
    return cellsInSeries > 0
        && capacity_Ah > 0.0
        && internalResistance25_Ohm > 0.0
        && polarizationResistance_Ohm >= 0.0 && polarizationTimeConstant_s > 0.0
        && maxDischargePower_W > 0.0
        && maxChargePower_W > 0.0
        && maxCurrent_A > 0.0
        && socMin >= 0.0 && socMax <= 1.0 && socMin < socMax
        && thermalCapacity_JperK > 0.0
        && heatTransfer_WperK >= 0.0;
}

double cellOpenCircuitVoltage(double soc)
{
    const double s = std::clamp(soc, 0.0, 1.0) * (kOcvPoints - 1);
    const int i = std::min(static_cast<int>(s), kOcvPoints - 2);
    const double frac = s - i;
    // Doğrusal ara değer: V = V_i + (V_{i+1} - V_i) * kesir
    return kCellOcvTable[i] + (kCellOcvTable[i + 1] - kCellOcvTable[i]) * frac;
}

double storedEnergyReleased_J(const BatteryParams& params, double socFrom, double socTo)
{
    // Yamuk kuralıyla sayısal integral; OCV tablosu parça parça doğrusal olduğundan
    // 1000 adım fazlasıyla yeterli (hata ~1e-7 bağıl).
    constexpr int kSteps = 1000;
    const double h = (socFrom - socTo) / kSteps;
    double integral = 0.0;
    for (int i = 0; i < kSteps; ++i) {
        const double s0 = socTo + h * i;
        integral += 0.5 * (cellOpenCircuitVoltage(s0) + cellOpenCircuitVoltage(s0 + h)) * h;
    }
    const double charge_C = params.capacity_Ah * 3600.0;
    return charge_C * params.cellsInSeries * integral;
}

Battery::Battery(const BatteryParams& params, double initialSoc, double initialTemp_C)
    : params_(params)
    , soc_(std::clamp(initialSoc, 0.0, 1.0))
    , temp_C_(initialTemp_C)
{
}

double Battery::openCircuitVoltage() const
{
    return cellOpenCircuitVoltage(soc_) * params_.cellsInSeries;
}

double Battery::internalResistance() const
{
    // R(T) = R25 * exp(k * (25 - T))
    return params_.internalResistance25_Ohm
         * std::exp(params_.resistanceTempCoeff_perK * (25.0 - temp_C_));
}

double Battery::availableDischargePower() const
{
    if (soc_ <= params_.socMin) {
        return 0.0;
    }
    // RC dalının gerilimi kaynaktan düşülür: E = OCV - V1
    const double ocv = openCircuitVoltage() - v1_;
    const double r = internalResistance();
    const double iMax = params_.maxCurrent_A;
    // Akım sınırındaki uç gücü: P = E*I - R*I^2
    const double pAtCurrentLimit = ocv * iMax - r * iMax * iMax;
    // Rint modelinin kuramsal tepe gücü OCV^2/(4R) (V = OCV/2 noktası)
    const double pTheoretical = ocv * ocv / (4.0 * r);
    return std::max(0.0, std::min({params_.maxDischargePower_W, pAtCurrentLimit, pTheoretical}));
}

double Battery::availableChargePower() const
{
    if (soc_ >= params_.socMax) {
        return 0.0;
    }
    const double ocv = openCircuitVoltage() - v1_;
    const double r = internalResistance();
    const double iMax = params_.maxCurrent_A;
    // Şarjda I = -iMax iken uç gücü büyüklüğü: |P| = iMax * (E + R*iMax)
    const double pAtCurrentLimit = iMax * (ocv + r * iMax);
    return std::min(params_.maxChargePower_W, pAtCurrentLimit);
}

BatteryStep Battery::step(double requestedPower_W, double dt_s, double ambientTemp_C)
{
    BatteryStep out;
    out.requestedPower_W = requestedPower_W;

    // 1) Gücü sınırla
    double power = requestedPower_W;
    if (power > 0.0) {
        power = std::min(power, availableDischargePower());
    } else if (power < 0.0) {
        power = std::max(power, -availableChargePower());
    }
    out.limited = (power != requestedPower_W);

    // 2) Elektriksel çözüm (adım başındaki SOC, sıcaklık ve V1 ile)
    const double ocv = openCircuitVoltage();
    const double r = internalResistance();
    const double current = currentForPower(ocv - v1_, r, power);
    out.power_W = power;
    out.current_A = current;
    out.ocv_V = ocv;
    out.voltage_V = ocv - current * r - v1_; // V = OCV - I*R0 - V1
    // İç kayıp = kimyasal güç - uç gücü = I*(OCV - V) = R0*I^2 + V1*I
    out.heat_W = current * (ocv - out.voltage_V);

    // RC dalı (I adım boyunca sabitken tam çözüm):
    //   V1' = V1*exp(-dt/tau) + R1(T)*I*(1 - exp(-dt/tau))
    const double tempFactor = r / params_.internalResistance25_Ohm;
    const double r1 = params_.polarizationResistance_Ohm * tempFactor;
    const double decay = std::exp(-dt_s / params_.polarizationTimeConstant_s);
    v1_ = v1_ * decay + r1 * current * (1.0 - decay);
    out.polarization_V = v1_;

    // 3) SOC güncelle (Coulomb sayımı):
    //    dSOC/dt = -I / Q,  Q = kapasite [Ah] * 3600 [C]
    const double charge_C = params_.capacity_Ah * 3600.0;
    soc_ -= current * dt_s / charge_C;
    // Adım içinde pencerenin biraz dışına taşma olabilir (sınır adım başında
    // kontrol edildi); fiziksel olarak 0..1 dışına çıkmasını engelle.
    soc_ = std::clamp(soc_, 0.0, 1.0);

    // 4) Sıcaklık güncelle (toplu ısıl model, açık Euler):
    //    C * dT/dt = I^2*R - hA * (T - T_ortam)
    //    Üretilen Joule ısısı paketi ısıtır, ortama olan ısı kaybı soğutur.
    const double dTdt = (out.heat_W - params_.heatTransfer_WperK * (temp_C_ - ambientTemp_C))
                      / params_.thermalCapacity_JperK;
    temp_C_ += dTdt * dt_s;

    out.soc = soc_;
    out.temp_C = temp_C_;
    return out;
}

} // namespace hea
