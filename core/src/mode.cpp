#include "hea/mode.h"

namespace hea {

const char* modeName(Mode mode)
{
    switch (mode) {
    case Mode::Stopped:      return "DUR";
    case Mode::Electric:     return "EV";
    case Mode::Engine:       return "ICE";
    case Mode::RegenBraking: return "REGEN";
    }
    return "?";
}

} // namespace hea
