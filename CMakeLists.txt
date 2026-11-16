cmake_minimum_required(VERSION 3.16)

project(HibritEnerjiAsistani
    VERSION 0.3.0
    DESCRIPTION "Akilli Hibrit Enerji Asistani - simulasyon"
    LANGUAGES CXX)

# Dil standardı: C++17 (derleyiciye özel uzantılar kapalı)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HEA_BUILD_TESTS "Birim testlerini derle" ON)
option(HEA_BUILD_GUI   "Dear ImGui + ImPlot arayuzunu derle" OFF)

# MSVC çalışma zamanı exe'nin içine statik bağlanır (/MT): program başka bir
# bilgisayarda "VCRUNTIME140.dll bulunamadı" hatası vermeden çalışır.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Tüm hedeflerde ortak uyarı ayarları
if(MSVC)
    # /utf-8: kaynak dosyalardaki Türkçe karakterler için
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)
add_subdirectory(apps/common)
add_subdirectory(apps/cli)

if(HEA_BUILD_GUI)
    add_subdirectory(apps/gui)
endif()

if(HEA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Dağıtım paketi (zip) ---
# cpack --config <derleme klasörü>/CPackConfig.cmake -C Release
# -> <derleme klasörü>/paket/HibritEnerjiAsistani-<sürüm>-windows-x64.zip
# İçerik: hea_cli.exe, hea_gui.exe + lisanslar (apps/gui/CMakeLists.txt), örnek veriler,
# BENIOKU.txt. CI, "v" ile başlayan etiketlerde bu zip'i GitHub Releases'e yükler.
install(DIRECTORY data/ DESTINATION data)
# Windows satır sonları: Not Defteri'nin eski sürümleri de düzgün göstersin
configure_file(packaging/BENIOKU.txt.in ${CMAKE_BINARY_DIR}/BENIOKU.txt @ONLY NEWLINE_STYLE CRLF)
install(FILES ${CMAKE_BINARY_DIR}/BENIOKU.txt DESTINATION .)

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_hea_platform windows-x64)
    else()
        set(_hea_platform windows-x86)
    endif()
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" _hea_platform)
endif()
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME HibritEnerjiAsistani)
set(CPACK_PACKAGE_FILE_NAME "HibritEnerjiAsistani-${PROJECT_VERSION}-${_hea_platform}")
# Zip açılınca dosyalar tek bir klasörün içinde çıksın
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/paket)
include(CPack)
