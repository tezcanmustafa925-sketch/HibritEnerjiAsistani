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
hea_add_test(test_config)
hea_add_test(test_extensions)

# --- Terminal uygulaması testleri ---
add_test(NAME cli_yardim COMMAND hea_cli --yardim)
add_test(NAME cli_varsayilan COMMAND hea_cli --hizli)
add_test(NAME cli_ozet COMMAND hea_cli --cevrim test --strateji hepsi)
add_test(NAME cli_tek_kosu_ozet COMMAND hea_cli --cevrim sehir_101 --ozet)
add_test(NAME cli_egitim_verisi
         COMMAND hea_cli --cevrim sehir_1 --egitim-verisi ${CMAKE_CURRENT_BINARY_DIR}/egitim_test.csv)
add_test(NAME cli_adimlar COMMAND hea_cli --cevrim sehir_101 --strateji kural --hizli --aralik 60)
add_test(NAME cli_bilinmeyen_hiz COMMAND hea_cli --hiz 0)
# Hatalı kullanım sıfırdan farklı çıkış kodu vermeli
add_test(NAME cli_bilinmeyen_secenek COMMAND hea_cli --bilinmeyen)
add_test(NAME cli_bilinmeyen_cevrim COMMAND hea_cli --cevrim yok_boyle_cevrim)
add_test(NAME cli_bilinmeyen_strateji COMMAND hea_cli --strateji yok)
add_test(NAME cli_hatali_sayi COMMAND hea_cli --aralik 5abc)
add_test(NAME cli_bilinmeyen_ayar COMMAND hea_cli --ayar batarya.yok=1 --ozet)
add_test(NAME cli_gecersiz_ayar COMMAND hea_cli --soc0 0.95 --ozet)
add_test(NAME cli_olmayan_ayar_dosyasi COMMAND hea_cli --ayar-dosyasi yok_boyle.ini --ozet)
add_test(NAME cli_olmayan_cevrim_dosyasi COMMAND hea_cli --cevrim-dosyasi yok_boyle.csv --ozet)
set_tests_properties(cli_bilinmeyen_secenek cli_bilinmeyen_cevrim cli_bilinmeyen_strateji
                     cli_bilinmeyen_hiz cli_hatali_sayi cli_bilinmeyen_ayar cli_gecersiz_ayar
                     cli_olmayan_ayar_dosyasi cli_olmayan_cevrim_dosyasi
                     PROPERTIES WILL_FAIL TRUE)
# Yeni seçenekler: ayar dosyası, CSV çevrim, standart çevrim, özet/karşılaştırma CSV'si
add_test(NAME cli_ayar_dosyasi
         COMMAND hea_cli --ayar-dosyasi ${PROJECT_SOURCE_DIR}/data/ornek_ayar.ini
                 --ayar batarya.kapasite_Ah=8 --soc0 0.55 --cevrim sehir_101 --ozet)
add_test(NAME cli_ayarlari_yaz
         COMMAND hea_cli --ayarlari-yaz ${CMAKE_CURRENT_BINARY_DIR}/ayarlar_test.ini)
add_test(NAME cli_cevrim_dosyasi
         COMMAND hea_cli --cevrim-dosyasi ${PROJECT_SOURCE_DIR}/data/cevrimler/ornek_tepe_yolu.csv
                 --strateji hepsi --ozet-csv ${CMAKE_CURRENT_BINARY_DIR}/ozet_test.csv)
add_test(NAME cli_nedc COMMAND hea_cli --cevrim nedc --strateji hepsi --ozet)
add_test(NAME cli_karsilastir_csv
         COMMAND hea_cli --cevrim nedc --strateji kural --karsilastir
                 --karsilastir-csv ${CMAKE_CURRENT_BINARY_DIR}/karsilastir_test.csv)
add_test(NAME cli_egitim_surucu_hepsi
         COMMAND hea_cli --cevrim sehir_1 --egitim-surucu hepsi
                 --egitim-verisi ${CMAKE_CURRENT_BINARY_DIR}/egitim_hepsi_test.csv)
# Depodaki örnek ayar dosyası güncel mi? (anahtar eklenince yeniden üretilmeli)
add_test(NAME ornek_ayar_guncel
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:hea_cli>
                 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/ornek_ayar_kontrol.ini
                 -DEXPECTED=${PROJECT_SOURCE_DIR}/data/ornek_ayar.ini
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/check_sample_config.cmake)
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
set(HEA_PYTHON_OK OFF)
if(Python3_Interpreter_FOUND)
    # Yorumlayıcı yetmez: paketler (model/requirements.txt) de kurulu olmalı,
    # yoksa test başarısız olmak yerine atlanır.
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import numpy, pandas, sklearn"
        RESULT_VARIABLE _hea_py_rc OUTPUT_QUIET ERROR_QUIET)
    if(_hea_py_rc EQUAL 0)
        set(HEA_PYTHON_OK ON)
    else()
        message(STATUS "Python paketleri eksik (pip install -r model/requirements.txt): "
                       "py_model_egitimi testi atlandi")
    endif()
endif()
if(HEA_PYTHON_OK)
    add_test(NAME py_model_egitimi
             COMMAND ${CMAKE_COMMAND}
                     -DCLI=$<TARGET_FILE:hea_cli>
                     -DPYTHON=${Python3_EXECUTABLE}
                     -DSCRIPT=${PROJECT_SOURCE_DIR}/model/egit.py
                     -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/model_test
                     -DREFERENCE=${PROJECT_SOURCE_DIR}/model/tuketim_modeli.json
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/check_model.cmake)
    # Uretilmis C++ katsayi dosyasi JSON ile guncel mi?
endif()
if(Python3_Interpreter_FOUND)
    # Yalnızca standart kütüphane kullanır; paketler olmadan da çalışır
    add_test(NAME py_katsayilar_guncel
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/model/cpp_aktar.py --kontrol)
else()
    message(STATUS "Python bulunamadi: Python testleri atlandi")
endif()
