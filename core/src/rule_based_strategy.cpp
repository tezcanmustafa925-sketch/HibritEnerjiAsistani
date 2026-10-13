#include "hea/rule_based_strategy.h"

#include <algorithm>

namespace hea {

bool RuleBasedParams::isValid() const
{
    return socLow < socTarget && socTarget < socHigh
        && evMaxSpeed_mps > speedHysteresis_mps && speedHysteresis_mps >= 0.0
        && evMaxPower_W > 0.0 && evMaxPowerHighSoc_W >= evMaxPower_W
        && powerHysteresis > 0.0 && powerHysteresis <= 1.0
        && minEngineOnTime_s >= 0.0 && chargeGain_W >= 0.0 && maxChargePower_W >= 0.0;
}

RuleBasedStrategy::RuleBasedStrategy(const RuleBasedParams& params)
    : p_(params)
{
}

void RuleBasedStrategy::reset()
{
    engineOn_ = false;
    engineOnTime_s_ = 0.0;
    lastTime_s_ = 0.0;
    hasLastTime_ = false;
}

double RuleBasedStrategy::evPowerLimit(double soc) const
{
    return soc > p_.socHigh ? p_.evMaxPowerHighSoc_W : p_.evMaxPower_W;
}

Decision RuleBasedStrategy::decide(const StrategyInput& in)
{
    // Önceki karardan bu yana geçen süre (motorun açık kalma süresi için)
    const double dt = hasLastTime_ ? std::max(0.0, in.time_s - lastTime_s_) : 0.0;
    lastTime_s_ = in.time_s;
    hasLastTime_ = true;
    if (engineOn_) {
        engineOnTime_s_ += dt;
    }

    // Fren, iniş, serbest yuvarlanma veya durma: tahrik gerekmiyor
    if (in.wheelPower_W <= 0.0) {
        engineOn_ = false;
        return Decision{Mode::Electric, 0.0};
    }

    const double limit = evPowerLimit(in.soc);
    const bool needEngine = in.soc < p_.socLow
                         || in.speed_mps > p_.evMaxSpeed_mps
                         || in.wheelPower_W > limit;
    const bool canStop = in.soc > p_.socLow + 0.05
                      && in.speed_mps < p_.evMaxSpeed_mps - p_.speedHysteresis_mps
                      && in.wheelPower_W < limit * p_.powerHysteresis;

    if (!engineOn_ && needEngine) {
        engineOn_ = true;
        engineOnTime_s_ = 0.0;
    } else if (engineOn_ && canStop && engineOnTime_s_ >= p_.minEngineOnTime_s) {
        engineOn_ = false;
    }

    if (!engineOn_) {
        return Decision{Mode::Electric, 0.0};
    }
    // P_sarj = kazanc * (SOC_hedef - SOC), [0, azami] aralığına kırpılır
    const double charge = std::clamp(p_.chargeGain_W * (p_.socTarget - in.soc),
                                     0.0, p_.maxChargePower_W);
    return Decision{Mode::Engine, charge};
}

} // namespace hea
