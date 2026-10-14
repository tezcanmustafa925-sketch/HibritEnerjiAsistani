# Dış bağımlılık olmadan basit testler: her test ayrı bir çalıştırılabilir dosya,
# CTest ile çalıştırılır. Çıkış kodu 0 değilse test başarısız sayılır.

function(hea_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE hea_core)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

hea_add_test(test_smoke)
hea_add_test(test_types)
hea_add_test(test_vehicle_model)
hea_add_test(test_battery)
hea_add_test(test_powertrain)
hea_add_test(test_drive_cycle)
hea_add_test(test_simulation)
hea_add_test(test_rule_based)
hea_add_test(test_training_data)
hea_add_test(test_model_based)
hea_add_test(test_comparison)

# --- Terminal uygulaması testleri ---
add_test(NAME cli_yardim COMMAND hea_cli --yardim)
add_test(NAME cli_ozet COMMAND hea_cli --cevrim sehir_101)
add_test(NAME cli_egitim_verisi
         COMMAND hea_cli --cevrim sehir_1 --egitim-verisi ${CMAKE_CURRENT_BINARY_DIR}/egitim_test.csv)
add_test(NAME cli_adimlar COMMAND hea_cli --cevrim sehir_101 --strateji kural --adimlar --aralik 60)
# Hatalı kullanım sıfırdan farklı çıkış kodu vermeli
add_test(NAME cli_bilinmeyen_secenek COMMAND hea_cli --bilinmeyen)
add_test(NAME cli_bilinmeyen_cevrim COMMAND hea_cli --cevrim yok_boyle_cevrim)
add_test(NAME cli_bilinmeyen_strateji COMMAND hea_cli --strateji yok)
set_tests_properties(cli_bilinmeyen_secenek cli_bilinmeyen_cevrim cli_bilinmeyen_strateji
                     PROPERTIES WILL_FAIL TRUE)
# CSV: sehir_101 1241 s -> 1241 adım satırı + başlık
add_test(NAME cli_karsilastir COMMAND hea_cli --cevrim sehir_101 --karsilastir)
add_test(NAME cli_csv
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:hea_cli>
                 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/cli_test.csv
                 -DEXPECTED_ROWS=1241
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/check_csv.cmake)

# --- Python model eğitimi (Python ve paketleri bulunursa) ---
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME py_model_egitimi
             COMMAND ${CMAKE_COMMAND}
                     -DCLI=$<TARGET_FILE:hea_cli>
                     -DPYTHON=${Python3_EXECUTABLE}
                     -DSCRIPT=${PROJECT_SOURCE_DIR}/model/egit.py
                     -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/model_test
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/check_model.cmake)
    # Uretilmis C++ katsayi dosyasi JSON ile guncel mi?
    add_test(NAME py_katsayilar_guncel
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/model/cpp_aktar.py --kontrol)
else()
    message(STATUS "Python bulunamadi: py_model_egitimi testi atlandi")
endif()
