// Duman testi: core kütüphanesi derleniyor, bağlanıyor ve çağrılabiliyor mu?

#include <cstring>

#include "hea/version.h"
#include "test_util.h"

int main()
{
    HEA_CHECK(hea::coreVersion() != nullptr);
    HEA_CHECK(std::strcmp(hea::coreVersion(), "0.1.0") == 0);
    // Test altyapısının kendisi: tolerans içi karşılaştırma çalışıyor mu?
    HEA_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-12);
    return HEA_TEST_RESULT();
}
