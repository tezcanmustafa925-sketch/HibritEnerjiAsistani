cmake_minimum_required(VERSION 3.16)

project(HibritEnerjiAsistani
    VERSION 0.2.0
    DESCRIPTION "Akilli Hibrit Enerji Asistani - simulasyon"
    LANGUAGES CXX)

# Dil standardı: C++17 (derleyiciye özel uzantılar kapalı)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HEA_BUILD_TESTS "Birim testlerini derle" ON)
option(HEA_BUILD_GUI   "Dear ImGui + ImPlot arayuzunu derle (henuz yok)" OFF)

# Tüm hedeflerde ortak uyarı ayarları
if(MSVC)
    # /utf-8: kaynak dosyalardaki Türkçe karakterler için
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)
add_subdirectory(apps/cli)

if(HEA_BUILD_GUI)
    message(FATAL_ERROR "apps/gui henuz eklenmedi.")
endif()

if(HEA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
