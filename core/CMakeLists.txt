# Çekirdek kütüphane: araç modeli, stratejiler, tahmin modeli, simülasyon.
# Kural: bu kütüphane stdout/stderr'e hiçbir şey yazmaz.
add_library(hea_core STATIC
    src/battery.cpp
    src/comparison.cpp
    src/config_file.cpp
    src/consumption_model.cpp
    src/cycle_io.cpp
    src/drive_cycle.cpp
    src/electric_motor.cpp
    src/engine.cpp
    src/mode.cpp
    src/model_based_strategy.cpp
    src/powertrain.cpp
    src/rule_based_strategy.cpp
    src/simulation.cpp
    src/strategy.cpp
    src/training_data.cpp
    src/units.cpp
    src/vehicle_model.cpp
    src/vehicle_params.cpp
    src/version.cpp
)

target_include_directories(hea_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(hea_core PUBLIC cxx_std_17)

# Sürüm tek kaynaktan: kök CMakeLists.txt'deki project(VERSION ...)
target_compile_definitions(hea_core PUBLIC HEA_VERSION_STRING="${PROJECT_VERSION}")
