#include "hea/vehicle_params.h"

namespace hea {

bool VehicleParams::isValid() const
{
    return curbMass_kg > 0.0
        && dragCoefficient > 0.0
        && frontalArea_m2 > 0.0
        && rollingResistanceCoeff > 0.0
        && wheelRadius_m > 0.0
        && rotationalMassFactor >= 1.0;
}

} // namespace hea
