#include "hea/model_based_strategy.h"

#include <algorithm>
#include <limits>

#include "hea/units.h"

namespace hea {

bool ModelBasedParams::isValid() const
{
    return socTarget > 0.0 && socTarget < 1.0 && equivalenceFactor > 0.0
        && socFeedbackGain >= 0.0 && socMinForEv >= 0.0 && socMinForEv < socTarget
        && chargePower_W >= 0.0 && engineStartFuel_kg >= 0.0 && minEngineOnTime_s > 0.0;
}

ModelBasedStrategy::ModelBasedStrategy(const ModelBasedParams& params,
                                       const ConsumptionModel& model)
    : p_(params)
    , model_(model)
{
}

void ModelBasedStrategy::reset()
{
    last_ = EcmsCosts{};
    engineOn_ = false;
    engineOnTime_s_ = 0.0;
    lastTime_s_ = 0.0;
    hasLastTime_ = false;
}

double ModelBasedStrategy::equivalenceFactor(double soc) const
{
    // s = s0 * (1 + k * (SOC_hedef - SOC))
    return std::max(0.0, p_.equivalenceFactor * (1.0 + p_.socFeedbackGain * (p_.socTarget - soc)));
}

Decision ModelBasedStrategy::decide(const StrategyInput& in)
{
    const double dt = hasLastTime_ ? std::max(0.0, in.time_s - lastTime_s_) : 0.0;
    lastTime_s_ = in.time_s;
    hasLastTime_ = true;
    if (engineOn_) {
        engineOnTime_s_ += dt;
    }

    // Fren, iniş, serbest yuvarlanma veya durma: tahrik gerekmiyor
    if (in.wheelPower_W <= 0.0) {
        engineOn_ = false;
        last_ = EcmsCosts{};
        return Decision{Mode::Electric, 0.0};
    }

    const double lhv = constants::kGasolineLhv;
    EcmsCosts c;
    c.s = equivalenceFactor(in.soc);

    // --- EV maliyeti: J_EV = s * P_bat / LHV ---
    const double pEv = model_.evBatteryPower_W(in);
    c.evAllowed = in.soc >= p_.socMinForEv && pEv <= in.batteryMaxDischarge_W;
    c.ev = c.evAllowed ? c.s * pEv / lhv : std::numeric_limits<double>::infinity();

    // --- ICE maliyeti: J_ICE = m_dot (+ motor kapalıysa çalıştırma payı) ---
    const double startShare = engineOn_ ? 0.0 : p_.engineStartFuel_kg / p_.minEngineOnTime_s;
    c.ice = model_.iceFuelRate_kgps(in) + startShare;

    // --- ICE + şarj: J = J_ICE + c * P_s - s * P_s / LHV ---
    const double pCharge = std::min(p_.chargePower_W, in.batteryMaxCharge_W);
    c.iceCharge = c.ice + (model_.chargeFuelPerJoule(in) - c.s / lhv) * pCharge;
    const bool chargeWorth = pCharge > 0.0 && c.iceCharge < c.ice;
    const double bestIce = chargeWorth ? c.iceCharge : c.ice;
    last_ = c;

    // Motor açıksa asgari süre dolana kadar açık kalır (sık aç-kapa önlenir)
    const bool mustStayOn = engineOn_ && engineOnTime_s_ < p_.minEngineOnTime_s;
    const bool useEngine = mustStayOn || bestIce < c.ev;

    if (!useEngine) {
        engineOn_ = false;
        return Decision{Mode::Electric, 0.0};
    }
    if (!engineOn_) {
        engineOn_ = true;
        engineOnTime_s_ = 0.0;
    }
    return Decision{Mode::Engine, chargeWorth ? pCharge : 0.0};
}

} // namespace hea
