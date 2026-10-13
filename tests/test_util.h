#pragma once

// Küçük test yardımcıları. Harici test kütüphanesi kullanmamak için yazıldı.
// Her kontrol başarısız olursa hata sayacını artırır; test sonunda
// HEA_TEST_RESULT() sayaç sıfır değilse 1 döndürür.

#include <cmath>
#include <cstdio>

namespace hea_test {
inline int& failureCount()
{
    static int count = 0;
    return count;
}
} // namespace hea_test

// Koşul doğru olmalı.
#define HEA_CHECK(cond)                                                        \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "%s:%d: KONTROL BASARISIZ: %s\n", __FILE__,   \
                         __LINE__, #cond);                                     \
            ++hea_test::failureCount();                                        \
        }                                                                      \
    } while (0)

// İki ondalıklı sayı, verilen mutlak tolerans içinde eşit olmalı.
#define HEA_CHECK_NEAR(actual, expected, tol)                                  \
    do {                                                                       \
        const double a_ = (actual);                                            \
        const double e_ = (expected);                                          \
        if (std::fabs(a_ - e_) > (tol)) {                                      \
            std::fprintf(stderr,                                               \
                         "%s:%d: KONTROL BASARISIZ: %s = %.9g, beklenen "      \
                         "%.9g (tol %.3g)\n",                                  \
                         __FILE__, __LINE__, #actual, a_, e_, (double)(tol));  \
            ++hea_test::failureCount();                                        \
        }                                                                      \
    } while (0)

#define HEA_TEST_RESULT() (hea_test::failureCount() == 0 ? 0 : 1)
