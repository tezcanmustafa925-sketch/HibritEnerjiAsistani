# Grafik arayüz: Dear ImGui + ImPlot, pencere için GLFW + OpenGL 3.
# Bağımlılıklar ilk yapılandırmada internetten indirilir (FetchContent);
# hepsi exe'ye statik bağlanır, dağıtımda ek DLL gerekmez.
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8
    GIT_SHALLOW    TRUE)
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16
    GIT_SHALLOW    TRUE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imgui implot glfw)

find_package(OpenGL REQUIRED)

# ImGui + ImPlot + GLFW/OpenGL3 arka uçları tek bir statik kütüphane
add_library(hea_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(hea_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
)
target_link_libraries(hea_imgui PUBLIC glfw OpenGL::GL)
# Üçüncü taraf kod: projenin sıkı uyarı ayarlarını uygulama
if(MSVC)
    target_compile_options(hea_imgui PRIVATE /W0)
else()
    target_compile_options(hea_imgui PRIVATE -w)
endif()

# Türkçe karakterli fontu (Roboto, Apache 2.0) derleme sırasında C++ dizisine
# çevirip exe'ye göm: program hiçbir font dosyası aramaz.
set(HEA_FONT_TTF ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf)
set(HEA_FONT_CPP ${CMAKE_CURRENT_BINARY_DIR}/embedded_font.cpp)
file(READ ${HEA_FONT_TTF} _font_hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _font_bytes "${_font_hex}")
string(LENGTH "${_font_hex}" _font_hex_len)
math(EXPR _font_size "${_font_hex_len} / 2")
file(WRITE ${HEA_FONT_CPP}
    "// Otomatik üretildi (apps/gui/CMakeLists.txt). Elle düzenlemeyin.\n"
    "#include \"embedded_font.h\"\n"
    "namespace gui {\n"
    "const unsigned char kFontData[] = {${_font_bytes}};\n"
    "const int kFontSize = ${_font_size};\n"
    "}\n")

# Arayüz mantığı (ImGui'siz): ayarlar -> simülasyon, PNG yazıcı.
# Birim testleri buna bağlanır (tests/test_gui_session.cpp).
add_library(hea_gui_session STATIC
    session.cpp
    live_run.cpp
    png_writer.cpp
)
target_include_directories(hea_gui_session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hea_gui_session PUBLIC hea_core hea_result_csv)

add_executable(hea_gui WIN32
    main.cpp
    app.cpp
    platform.cpp
    ${HEA_FONT_CPP}
)
target_include_directories(hea_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hea_gui PRIVATE hea_gui_session hea_imgui)

if(WIN32)
    # Dosya açma penceresi (GetOpenFileNameW), geniş karakterli komut satırı
    target_link_libraries(hea_gui PRIVATE comdlg32 shell32)
endif()

# Dağıtım paketinde (zip): exe kök klasöre, exe'ye gömülü bileşenlerin lisansları
# lisanslar/ klasörüne (MIT ve Apache 2.0 lisansları metnin dağıtılmasını ister)
install(TARGETS hea_gui RUNTIME DESTINATION .)
install(FILES ${imgui_SOURCE_DIR}/LICENSE.txt DESTINATION lisanslar RENAME imgui_LICENSE.txt)
install(FILES ${implot_SOURCE_DIR}/LICENSE DESTINATION lisanslar RENAME implot_LICENSE.txt)
install(FILES ${glfw_SOURCE_DIR}/LICENSE.md DESTINATION lisanslar RENAME glfw_LICENSE.txt)
install(FILES ${PROJECT_SOURCE_DIR}/packaging/lisanslar/Roboto_LICENSE.txt DESTINATION lisanslar)

if(MSVC)
    # WIN32 alt sistemi: konsol penceresi açılmaz; giriş noktası yine main()
    target_link_options(hea_gui PRIVATE /ENTRY:mainCRTStartup)
    target_compile_definitions(hea_gui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
