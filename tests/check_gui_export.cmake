# Arayüzün "CSV olarak kaydet" / "Görüntü kaydet" düğmeleri (cmake -P).
# Arayüz görünmez pencerede, --kayit-klasoru ile (dosya penceresi açılmadan) Türkçe
# karakterli bir klasöre kaydeder; dosyalar aynı ayarlarla hea_cli'nin yazdıklarıyla
# BAYT BAYT aynı olmalı:
#   adımlar        = hea_cli --csv
#   karşılaştırma  = hea_cli --strateji hepsi --karsilastir --karsilastir-csv
# Ekran görüntüsü: geçerli PNG, en az pencere boyutunda.
# Girdi değişkenleri:
#   CLI, GUI   hea_cli ve hea_gui yolları
#   OUT        çıktıların ortak öneki (klasör: OUT_kayıt_ğüş)
#   ARGS       ikisine de verilecek argümanlar (| ile ayrılmış)
#   STRATEGY   tek koşunun stratejisi (ice | ev | kural | model; ikisinde de aynı ad)

string(REPLACE "|" ";" argList "${ARGS}")
set(dir "${OUT}_kayıt_ğüş")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
file(REMOVE "${OUT}_cli_adimlar.csv" "${OUT}_cli_kars.csv")

execute_process(
    COMMAND "${CLI}" ${argList} --strateji ${STRATEGY} --hizli --csv "${OUT}_cli_adimlar.csv"
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 50)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hea_cli (adimlar) cikis kodu '${rc}': ${err}")
endif()
execute_process(
    COMMAND "${CLI}" ${argList} --strateji hepsi --karsilastir --karsilastir-csv "${OUT}_cli_kars.csv"
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 50)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hea_cli (karsilastirma) cikis kodu '${rc}': ${err}")
endif()

# Sürüş sekmesinde CSV + görüntü, sonra karşılaştırma sekmesinde CSV
execute_process(
    COMMAND "${GUI}" ${argList} --strateji ${STRATEGY} --kayit-klasoru "${dir}"
            --boyut 1400x860 --ekran-goruntusu "${OUT}.png"
            --eylem tikla:csv_kaydet --eylem tikla:goruntu_kaydet
            --eylem tikla:sekme_karsilastirma --eylem tikla:csv_kaydet
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 50)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hea_gui cikis kodu '${rc}': ${err}\n${out}")
endif()
message(STATUS "Durum: ${out}")
foreach(part "kayit=" "hata=-")
    string(FIND "${out}" "${part}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Durum satirinda '${part}' yok:\n${out}")
    endif()
endforeach()

file(GLOB steps "${dir}/*_adimlar.csv")
file(GLOB comparison "${dir}/*_karsilastirma.csv")
file(GLOB pngs "${dir}/*.png")
foreach(var steps comparison pngs)
    list(LENGTH ${var} n)
    if(NOT n EQUAL 1)
        file(GLOB all "${dir}/*")
        message(FATAL_ERROR "Kayit klasorunde tek '${var}' dosyasi bekleniyordu: ${all}")
    endif()
endforeach()

foreach(pair "${steps}|${OUT}_cli_adimlar.csv" "${comparison}|${OUT}_cli_kars.csv")
    string(REPLACE "|" ";" files "${pair}")
    list(GET files 0 guiFile)
    list(GET files 1 cliFile)
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${guiFile}" "${cliFile}"
                    RESULT_VARIABLE same)
    if(NOT same EQUAL 0)
        message(FATAL_ERROR "Arayuzun dosyasi CLI ile ayni degil:\n  ${guiFile}\n  ${cliFile}")
    endif()
    file(SIZE "${guiFile}" size)
    message(STATUS "Bayt bayt ayni (${size} bayt): ${guiFile}")
endforeach()

# PNG imzası ve IHDR'deki boyut (bayt 16..23, büyük uçlu)
file(READ "${pngs}" head LIMIT 24 HEX)
string(SUBSTRING "${head}" 0 16 sig)
if(NOT sig STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "PNG imzasi yanlis: ${sig}")
endif()
string(SUBSTRING "${head}" 32 8 wHex)
string(SUBSTRING "${head}" 40 8 hHex)
math(EXPR w "0x${wHex}")
math(EXPR h "0x${hHex}")
if(w LESS 1400 OR h LESS 860)
    message(FATAL_ERROR "Ekran goruntusu boyutu ${w}x${h}, beklenen en az 1400x860")
endif()
message(STATUS "Ekran goruntusu: ${pngs} (${w}x${h})")
