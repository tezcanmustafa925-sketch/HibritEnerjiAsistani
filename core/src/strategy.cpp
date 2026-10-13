#include "hea/strategy.h"

namespace hea {

const char* FixedModeStrategy::name() const
{
    return mode_ == Mode::Engine ? "sadece-ICE" : "EV-oncelikli";
}

Decision FixedModeStrategy::decide(const StrategyInput& /*in*/)
{
    Decision d;
    d.mode = mode_;
    return d;
}

} // namespace hea
