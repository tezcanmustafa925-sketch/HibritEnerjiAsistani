# Çekirdek kütüphane: araç modeli, stratejiler, tahmin modeli, simülasyon.
# Kural: bu kütüphane stdout/stderr'e hiçbir şey yazmaz.
add_library(hea_core STATIC
    src/version.cpp
)

target_include_directories(hea_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(hea_core PUBLIC cxx_std_17)
