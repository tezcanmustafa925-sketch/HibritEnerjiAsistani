# Arayüzün strateji karşılaştırması hea_cli --karsilastir ile birebir aynı mı? (cmake -P)
# Aynı argümanlarla önce CLI karşılaştırma CSV'sini yazar, sonra arayüzü görünmez
# pencerede açıp karşılaştırma sekmesine tıklar; her strateji için SOC düzeltmeli ve
# eşdeğer L/100 km değerleri (ikisi de %.9g) metin olarak aynı olmalı.
# Girdi değişkenleri:
#   CLI, GUI   hea_cli ve hea_gui yolları
#   OUT        çıktı dosyalarının ortak öneki (OUT.csv, OUT.png)
#   ARGS       ikisine de verilecek argümanlar (| ile ayrılmış; ör. --cevrim|nedc)

string(REPLACE "|" ";" argList "${ARGS}")
file(REMOVE "${OUT}.csv" "${OUT}.png")

execute_process(
    COMMAND "${CLI}" ${argList} --strateji hepsi --karsilastir --karsilastir-csv "${OUT}.csv"
    RESULT_VARIABLE rc
    OUTPUT_QUIET
    ERROR_VARIABLE err
    TIMEOUT 50)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hea_cli cikis kodu '${rc}': ${err}")
endif()

execute_process(
    COMMAND "${GUI}" ${argList} --boyut 1400x860 --ekran-goruntusu "${OUT}.png"
            --eylem tikla:sekme_karsilastirma
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    TIMEOUT 50)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hea_gui cikis kodu '${rc}': ${err}\n${out}")
endif()
message(STATUS "Durum: ${out}")
foreach(part "sekme=karsilastirma" "kars_kirli=0" "hata=-")
    string(FIND "${out}" "${part}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Durum satirinda '${part}' yok:\n${out}")
    endif()
endforeach()

# CSV sütunları: 1 strategy, 9 corrected_fuel_per_100km_L, 14 equivalent_fuel_per_100km_L
file(STRINGS "${OUT}.csv" lines)
list(LENGTH lines n)
if(NOT n EQUAL 5)
    message(FATAL_ERROR "CSV'de baslik + 4 strateji bekleniyordu, ${n} satir var")
endif()
list(REMOVE_AT lines 0)
foreach(line IN LISTS lines)
    string(REPLACE "," ";" cols "${line}")
    list(GET cols 1 strategy)
    list(GET cols 9 corrected)
    list(GET cols 14 equivalent)
    set(expected "kars_${strategy}=${corrected}/${equivalent}")
    string(FIND "${out}" "${expected}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Arayuz CLI ile ayni degil: '${expected}' bekleniyordu:\n${out}")
    endif()
    message(STATUS "Ayni: ${expected}")
endforeach()
