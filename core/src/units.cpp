#include "hea/units.h"

#include <cmath>

namespace hea {
namespace units {

double gradePercentToRad(double percent)
{
    // %p eğim: 100 m yatayda p m yükselme -> theta = atan(p / 100)
    return std::atan(percent / 100.0);
}

} // namespace units
} // namespace hea
