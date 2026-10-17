#include "hea/version.h"

namespace hea {

const char* coreVersion()
{
    return HEA_VERSION_STRING; // CMake project(VERSION ...) değerinden gelir
}

} // namespace hea
