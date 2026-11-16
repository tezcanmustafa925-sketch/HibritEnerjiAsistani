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

# --- Dağıtım: exe'ler yalnızca her Windows'ta bulunan DLL'lere mi bağlı? ---
# (MSVC; dumpbin bağlayıcıyla aynı klasördedir. Ekran kartı gerekmez, CI'da da çalışır.)
if(MSVC)
    get_filename_component(_hea_linker_dir "${CMAKE_LINKER}" DIRECTORY)
    find_program(HEA_DUMPBIN dumpbin HINTS "${_hea_linker_dir}")
    if(HEA_DUMPBIN)
        set(_hea_dist_files "$<TARGET_FILE:hea_cli>")
        if(HEA_BUILD_GUI)
            string(APPEND _hea_dist_files "|$<TARGET_FILE:hea_gui>")
        endif()
        add_test(NAME paket_dll_bagimliliklari
                 COMMAND ${CMAKE_COMMAND}
                         -DDUMPBIN=${HEA_DUMPBIN}
                         "-DFILES=${_hea_dist_files}"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/check_dependencies.cmake)
    else()
        message(STATUS "dumpbin bulunamadi: paket_dll_bagimliliklari testi atlandi")
    endif()
endif()

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

# --- Grafik arayüz (HEA_BUILD_GUI=ON ise) ---
if(HEA_BUILD_GUI)
    # Arayüz mantığı: ayarlar -> simülasyon, dosya okuma, PNG yazıcı (ImGui gerekmez)
    add_executable(test_gui_session test_gui_session.cpp)
    target_link_libraries(test_gui_session PRIVATE hea_gui_session)
    target_include_directories(test_gui_session PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(test_gui_session PRIVATE HEA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME test_gui_session COMMAND test_gui_session)

    # Arayüzü görünmez pencerede çizip ekran görüntüsü alır. OpenGL 3 destekli
    # ekran kartı gerekir (GitHub'ın Windows sanal makinelerinde yoktur).
    # args / args_after: --ekran-goruntusu'ndan önce / sonra gelen argümanlar (| ile ayrılmış)
    function(hea_add_gui_test name args args_after expect_rc no_png)
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND}
                         -DGUI=$<TARGET_FILE:hea_gui>
                         -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.png
                         "-DARGS=${args}"
                         "-DARGS_AFTER=${args_after}"
                         -DWIDTH=1400 -DHEIGHT=860
                         -DEXPECT_RC=${expect_rc}
                         -DEXPECT_NO_PNG=${no_png}
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/check_gui_png.cmake)
        # Görünmez modda bir saniyenin altında biter; takılırsa test düşsün
        set_tests_properties(${name} PROPERTIES LABELS gui_ekran TIMEOUT 30)
    endfunction()
    hea_add_gui_test(gui_ekran_varsayilan "" "" 0 OFF)
    hea_add_gui_test(gui_ekran_nedc_kural "--cevrim|nedc|--strateji|kural" "" 0 OFF)
    hea_add_gui_test(gui_ekran_dosyalar
        "--cevrim-dosyasi|${PROJECT_SOURCE_DIR}/data/cevrimler/ornek_tepe_yolu.csv|--ayar-dosyasi|${PROJECT_SOURCE_DIR}/data/ornek_ayar.ini"
        "" 0 OFF)
    # Bilinmeyen çevrim: arayüz hatayı gösterir (çıkış kodu 4), görüntü yine yazılır
    hea_add_gui_test(gui_ekran_hatali_cevrim "--cevrim|boyle_cevrim_yok" "" 4 OFF)
    # Argüman hataları: çıkış kodu 2, görüntü yok, görünür pencere AÇILMAZ.
    # Hatalı argüman --ekran-goruntusu'ndan önce de gelse sonra da gelse.
    hea_add_gui_test(gui_hatali_secenek_once "--boyle-secenek-yok" "" 2 ON)
    hea_add_gui_test(gui_hatali_secenek_sonra "" "--boyle-secenek-yok" 2 ON)
    hea_add_gui_test(gui_degersiz_secenek "" "--cevrim" 2 ON)
    hea_add_gui_test(gui_hatali_strateji "--strateji|hepsi" "" 4 OFF)
    # Var olmayan kontrole tıklama: çıkış kodu 6
    hea_add_gui_test(gui_hatali_eylem "--eylem|tikla:boyle_kontrol_yok" "" 6 ON)

    # Geçersiz bekleme / fare konumu: çıkış kodu 6
    hea_add_gui_test(gui_hatali_bekle "--eylem|bekle:0" "" 6 ON)
    hea_add_gui_test(gui_hatali_fare "--eylem|fare:abc" "" 6 ON)

    # OpenGL 3 yoksa (eski sürücü, Uzak Masaüstü, sanal makine): çıkış kodu 3 ve açıklama.
    # Hiçbir sürücüde olmayan OpenGL 9.9 istenerek üretilir. Normal modda kullanıcıya
    # hata kutusu da gösterilir; ekran görüntüsü modunda kutu açılmaz.
    add_test(NAME gui_opengl_yok
             COMMAND ${CMAKE_COMMAND}
                     -DGUI=$<TARGET_FILE:hea_gui>
                     -DOUT=${CMAKE_CURRENT_BINARY_DIR}/gui_opengl_yok.png
                     -DWIDTH=1400 -DHEIGHT=860
                     -DEXPECT_RC=3 -DEXPECT_NO_PNG=ON
                     "-DEXPECT_ERROR=GLFW hatasi|Grafik baslatilamadi: OpenGL 3.0"
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/check_gui_png.cmake)
    set_tests_properties(gui_opengl_yok PROPERTIES
                         LABELS gui_ekran TIMEOUT 30
                         ENVIRONMENT HEA_TEST_OPENGL_SURUMU=9.9)

    # --- Etkileşim senaryoları: fare/klavye olayları taklit edilir, son durum denetlenir ---
    # actions: --eylem değerleri (| ile ayrılmış); expect: durum satırında olması gerekenler;
    # isteğe bağlı 5. argüman: durum satırının uyması gereken düzenli ifade
    function(hea_add_gui_scenario name start_args actions expect)
        set(regex "")
        if(ARGC GREATER 4)
            set(regex "${ARGV4}")
        endif()
        set(args "${start_args}")
        string(REPLACE "|" ";" actionList "${actions}")
        foreach(a IN LISTS actionList)
            if(args STREQUAL "")
                set(args "--eylem|${a}")
            else()
                set(args "${args}|--eylem|${a}")
            endif()
        endforeach()
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND}
                         -DGUI=$<TARGET_FILE:hea_gui>
                         -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.png
                         "-DARGS=${args}"
                         -DWIDTH=1400 -DHEIGHT=860
                         "-DEXPECT_OUTPUT=${expect}"
                         "-DEXPECT_REGEX=${regex}"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/check_gui_png.cmake)
        set_tests_properties(${name} PROPERTIES LABELS gui_ekran TIMEOUT 30)
    endfunction()

    # sehir_101 + model tabanlı, varsayılan ayarlar: açılıştaki anında çalıştırmanın
    # sonucu. Canlı oynatma sonuna kadar gidince aynısı çıkmalı.
    set(HEA_SEHIR101_SONUC "yakit_L=0.2757 soc_son=0.5939")

    # Açılış: varsayılan çevrim ve strateji otomatik, anında çalışır
    hea_add_gui_scenario(gui_senaryo_acilis "" ""
        "cevrim=sehir_101 strateji=model-tabanli soc0=0.60|sonuc=sehir_101/model-tabanli|${HEA_SEHIR101_SONUC}|canli=yok adim=1241/1241 hiz=10|sekme=surus kars=yok|kirli=0|hata=-")
    # Strateji listesini aç, "Kural tabanlı"yı seç, Çalıştır'a tıkla
    hea_add_gui_scenario(gui_senaryo_strateji ""
        "tikla:strateji|tikla:strateji/2|tikla:calistir"
        "strateji=kural-tabanli|sonuc=sehir_101/kural-tabanli|kirli=0|hata=-")
    # Seçim değişip çalıştırılmazsa sonuç eski kalır ve "kirli" işaretlenir
    hea_add_gui_scenario(gui_senaryo_kirli ""
        "tikla:strateji|tikla:strateji/0"
        "strateji=sadece-ICE|sonuc=sehir_101/model-tabanli|kirli=1")
    # Çevrim listesinden NEDC'yi seç, F5 ile çalıştır
    hea_add_gui_scenario(gui_senaryo_cevrim_f5 ""
        "tikla:cevrim|tikla:cevrim/nedc|tus:F5"
        "cevrim=nedc|sonuc=nedc/model-tabanli|kirli=0|hata=-")
    # SOC kaydırıcısı: en sola tıklama alt sınır (0.30), en sağa üst sınır (0.80)
    hea_add_gui_scenario(gui_senaryo_soc_alt "" "kaydir:soc:0|tikla:calistir"
        "soc0=0.30|sonuc_soc0=0.30|kirli=0")
    hea_add_gui_scenario(gui_senaryo_soc_ust "" "kaydir:soc:1|tikla:calistir"
        "soc0=0.80|sonuc_soc0=0.80|kirli=0")
    # Koşullar: sabit sıcaklık 45 °C, sabit yük 0 kg, klima ve soğuk başlangıç kapalı
    hea_add_gui_scenario(gui_senaryo_kosullar ""
        "tikla:sabit_ortam|kaydir:ortam:1|tikla:sabit_yuk|kaydir:yuk:0|tikla:klima|tikla:soguk|tikla:calistir"
        "ortam=sabit:45|yuk=sabit:0|klima=0|soguk=0|kirli=0|hata=-")
    # Sabit sıcaklık kapalıyken kaydırıcı devre dışıdır: tıklama değeri değiştirmez
    hea_add_gui_scenario(gui_senaryo_devre_disi "" "kaydir:ortam:1|kaydir:yuk:1"
        "ortam=cevrim:25|yuk=cevrim:75|kirli=0")
    # Ayar dosyası ve CSV çevrimiyle açılış
    hea_add_gui_scenario(gui_senaryo_dosyalar
        "--ayar-dosyasi|${PROJECT_SOURCE_DIR}/data/ornek_ayar.ini|--cevrim-dosyasi|${PROJECT_SOURCE_DIR}/data/cevrimler/ornek_tepe_yolu.csv"
        ""
        "cevrim=ornek_tepe_yolu|ayar=ornek_ayar.ini|sonuc=ornek_tepe_yolu/model-tabanli|kirli=0|hata=-")
    # Ayar dosyasıyla açılış, sonra "Varsayılana dön"
    hea_add_gui_scenario(gui_senaryo_varsayilan
        "--ayar-dosyasi|${PROJECT_SOURCE_DIR}/data/ornek_ayar.ini" "tikla:varsayilan"
        "ayar=varsay|kirli=1|hata=-")

    # --- Canlı oynatma. Ekran görüntüsü modunda her kare 1/60 s sayılır: 10 kat hızda
    # kare başına 1/6 adım, 100 kat hızda 5/3 adım (sehir_101: 1241 adım, adım 1 s).
    # "adim=[1-9][0-9]?/1241 " = ilerlemiş ama 100 adımı geçmemiş (duraklatma işe
    # yaramasaydı yüzlerce adım olurdu).
    # Oynat (10 kat): ilerliyor, bitmedi
    hea_add_gui_scenario(gui_senaryo_canli_oynat "" "tikla:canli|bekle:30"
        "canli=oynuyor|hiz=10 |sonuc=sehir_101/model-tabanli|kirli=0|hata=-"
        "adim=[1-9][0-9]?/1241 ")
    # Hız kaydırıcısı en solda 1 kat: 120 kare = 2 s -> birkaç adım
    hea_add_gui_scenario(gui_senaryo_canli_yavas "" "kaydir:hiz:0|tikla:canli|bekle:120"
        "canli=oynuyor|hiz=1 |hata=-" "adim=[1-9]/1241 ")
    # Duraklat düğmesi (100 kat): duraklatılınca 200 kare boyunca ilerlemez
    hea_add_gui_scenario(gui_senaryo_canli_duraklat ""
        "kaydir:hiz:1|tikla:canli|bekle:20|tikla:duraklat|bekle:200"
        "canli=duraklatildi|hiz=100|hata=-" "adim=[1-9][0-9]?/1241 ")
    # Aynı düğme ("Devam et") sürdürür
    hea_add_gui_scenario(gui_senaryo_canli_devam ""
        "kaydir:hiz:1|tikla:canli|bekle:20|tikla:duraklat|bekle:100|tikla:duraklat|bekle:10"
        "canli=oynuyor|hiz=100|hata=-" "adim=[1-9][0-9]?/1241 ")
    # Boşluk tuşu: duraklat, sonra devam et
    hea_add_gui_scenario(gui_senaryo_canli_bosluk ""
        "kaydir:hiz:1|tikla:canli|bekle:20|tus:Space|bekle:100|tus:Space|bekle:20"
        "canli=oynuyor|hata=-" "adim=[1-9][0-9]?/1241 ")
    # Sona git: kalan adımlar hemen; sonuç anında çalıştırmayla aynı
    hea_add_gui_scenario(gui_senaryo_canli_sona_git "" "tikla:canli|bekle:5|tikla:sona_git"
        "canli=bitti adim=1241/1241|${HEA_SEHIR101_SONUC}|kirli=0|hata=-")
    # 100 kat hızda kendiliğinden biter (~745 kare); sonuç yine aynı
    hea_add_gui_scenario(gui_senaryo_canli_sonuna_kadar "" "kaydir:hiz:1|tikla:canli|bekle:760"
        "canli=bitti adim=1241/1241|${HEA_SEHIR101_SONUC}|hata=-")
    # Baştan oynat: ~500 adımdayken yeniden başlar
    hea_add_gui_scenario(gui_senaryo_canli_bastan ""
        "kaydir:hiz:1|tikla:canli|bekle:300|tikla:canli|bekle:10"
        "canli=oynuyor|hata=-" "adim=[1-9][0-9]?/1241 ")
    # Oynarken F5: canlı oynatma bırakılır, sonuç hemen hesaplanır
    hea_add_gui_scenario(gui_senaryo_canli_f5 "" "tikla:canli|bekle:30|tus:F5"
        "canli=yok adim=1241/1241|${HEA_SEHIR101_SONUC}|kirli=0|hata=-")
    # Oynarken ayar değişirse oynatma başladığı ayarlarla sürer, sonuç "eski" işaretlenir
    hea_add_gui_scenario(gui_senaryo_canli_kirli ""
        "tikla:canli|bekle:30|tikla:strateji|tikla:strateji/0|bekle:30"
        "strateji=sadece-ICE|sonuc=sehir_101/model-tabanli|canli=oynuyor|kirli=1|hata=-")

    # --- Strateji karşılaştırması sekmesi ---
    # hea_cli --karsilastir ile birebir aynı sonuç (sayılar CLI'dan okunur, elle yazılmaz)
    function(hea_add_gui_compare_test name args)
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND}
                         -DCLI=$<TARGET_FILE:hea_cli>
                         -DGUI=$<TARGET_FILE:hea_gui>
                         -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}
                         "-DARGS=${args}"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/check_gui_compare.cmake)
        set_tests_properties(${name} PROPERTIES LABELS gui_ekran TIMEOUT 60)
    endfunction()
    hea_add_gui_compare_test(gui_karsilastirma_sehir101 "--cevrim|sehir_101")
    hea_add_gui_compare_test(gui_karsilastirma_nedc_ayar
        "--cevrim|nedc|--ayar-dosyasi|${PROJECT_SOURCE_DIR}/data/ornek_ayar.ini")
    hea_add_gui_compare_test(gui_karsilastirma_csv_cevrim
        "--cevrim-dosyasi|${PROJECT_SOURCE_DIR}/data/cevrimler/ornek_tepe_yolu.csv")

    # --- Dışa aktarma: "CSV olarak kaydet" / "Görüntü kaydet" (--kayit-klasoru ile dosya
    # penceresi açılmadan). CSV'ler aynı ayarlarla hea_cli çıktısıyla bayt bayt aynı olmalı.
    function(hea_add_gui_export_test name args strategy)
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND}
                         -DCLI=$<TARGET_FILE:hea_cli>
                         -DGUI=$<TARGET_FILE:hea_gui>
                         -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}
                         "-DARGS=${args}"
                         -DSTRATEGY=${strategy}
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/check_gui_export.cmake)
        set_tests_properties(${name} PROPERTIES LABELS gui_ekran TIMEOUT 60)
    endfunction()
    hea_add_gui_export_test(gui_disa_aktarma_sehir101 "--cevrim|sehir_101" model)
    hea_add_gui_export_test(gui_disa_aktarma_nedc_ayar
        "--cevrim|nedc|--ayar-dosyasi|${PROJECT_SOURCE_DIR}/data/ornek_ayar.ini" kural)
    # Yazılamayan klasör: arayüz hatayı gösterir (çıkış kodu 4), program çökmez
    hea_add_gui_test(gui_disa_aktarma_hatali_klasor
        "--kayit-klasoru|${CMAKE_CURRENT_BINARY_DIR}/boyle_klasor_yok|--eylem|tikla:csv_kaydet"
        "" 4 OFF)

    # Sekmeye gelince ayar değişirse karşılaştırma "eski" işaretlenir
    hea_add_gui_scenario(gui_senaryo_kars_kirli ""
        "tikla:sekme_karsilastirma|tikla:sabit_yuk|kaydir:yuk:1"
        "sekme=karsilastirma|kars=sehir_101 kars_kirli=1|kirli=1|hata=-")
    # Karşılaştırma sekmesinde F5 karşılaştırmayı yeniler, tek çalıştırmaya dokunmaz
    hea_add_gui_scenario(gui_senaryo_kars_f5 ""
        "tikla:sekme_karsilastirma|tikla:sabit_yuk|kaydir:yuk:1|tus:F5"
        "sekme=karsilastirma|kars_kirli=0|yuk=sabit:500|kirli=1|hata=-")
    # Sol alttaki düğme bu sekmede "Karşılaştır": aynı iş
    hea_add_gui_scenario(gui_senaryo_kars_dugme ""
        "tikla:sekme_karsilastirma|kaydir:soc:0|tikla:calistir"
        "sekme=karsilastirma|soc0=0.30|kars_kirli=0|kirli=1|hata=-")
    # Strateji seçimi karşılaştırmayı eskitmez (4 strateji birden karşılaştırılıyor)
    hea_add_gui_scenario(gui_senaryo_kars_strateji ""
        "tikla:sekme_karsilastirma|tikla:strateji|tikla:strateji/0"
        "strateji=sadece-ICE|kars_kirli=0|kirli=1|hata=-")
    # Sürüş sekmesine dönünce F5 yine tek çalıştırma yapar; karşılaştırma kalır
    hea_add_gui_scenario(gui_senaryo_kars_geri ""
        "tikla:sekme_karsilastirma|tikla:sabit_yuk|tikla:sekme_surus|tus:F5"
        "sekme=surus|kars=sehir_101 kars_kirli=1|kirli=0|hata=-")
endif()
