# Uygulamaların (hea_cli, hea_gui) ortak kodu: sonuç CSV biçimleri.
# Core'a konmadı: core dosya yazmaz (yalnızca çevrim / ayar dosyası okur ve hesaplar).
add_library(hea_result_csv STATIC
    result_csv.cpp
)
target_include_directories(hea_result_csv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hea_result_csv PUBLIC hea_core)
