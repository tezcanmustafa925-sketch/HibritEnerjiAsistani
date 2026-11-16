# Arayüzü görünmez pencerede çizdirip ekran görüntüsünü kontrol eden betik (cmake -P).
# Girdi değişkenleri:
#   GUI         hea_gui yolu
#   OUT         PNG yolu
#   ARGS        --ekran-goruntusu'ndan ÖNCE verilecek argümanlar (| ile ayrılmış, isteğe bağlı)
#   ARGS_AFTER  --ekran-goruntusu'ndan SONRA verilecek argümanlar (| ile ayrılmış, isteğe bağlı)
#   WIDTH, HEIGHT  beklenen görüntü boyutu (--boyut ile verilir)
#   EXPECT_RC   beklenen çıkış kodu (varsayılan 0)
#   EXPECT_NO_PNG  ON ise görüntü OLUŞMAMALI (argüman hatası)
#   EXPECT_OUTPUT  hea_gui'nin durum satırında bulunması gereken parçalar (| ile ayrılmış,
#                  düz metin; ör. "strateji=kural-tabanli|kirli=0")
#   EXPECT_REGEX   durum satırının uyması gereken düzenli ifade (isteğe bağlı;
#                  ör. "adim=[1-9][0-9]?/1241 " = ilerlemiş ama 100 adımı geçmemiş)
#   EXPECT_ERROR   stderr'de bulunması gereken parçalar (| ile ayrılmış, isteğe bağlı)

if(NOT DEFINED EXPECT_RC)
    set(EXPECT_RC 0)
endif()

string(REPLACE "|" ";" argList "${ARGS}")
string(REPLACE "|" ";" argAfterList "${ARGS_AFTER}")
file(REMOVE "${OUT}")
execute_process(
    COMMAND "${GUI}" ${argList} --boyut ${WIDTH}x${HEIGHT} --ekran-goruntusu "${OUT}" ${argAfterList}
    RESULT_VARIABLE rc
    ERROR_VARIABLE err
    OUTPUT_VARIABLE out
    TIMEOUT 25)
if(NOT rc EQUAL EXPECT_RC)
    message(FATAL_ERROR "hea_gui cikis kodu '${rc}', beklenen ${EXPECT_RC}: ${err}\n${out}")
endif()
message(STATUS "Durum: ${out}")

if(DEFINED EXPECT_ERROR AND NOT EXPECT_ERROR STREQUAL "")
    string(REPLACE "|" ";" expectErrList "${EXPECT_ERROR}")
    foreach(part IN LISTS expectErrList)
        string(FIND "${err}" "${part}" pos)
        if(pos EQUAL -1)
            message(FATAL_ERROR "stderr'de '${part}' yok:\n${err}")
        endif()
    endforeach()
endif()

if(DEFINED EXPECT_OUTPUT AND NOT EXPECT_OUTPUT STREQUAL "")
    string(REPLACE "|" ";" expectList "${EXPECT_OUTPUT}")
    foreach(part IN LISTS expectList)
        string(FIND "${out}" "${part}" pos)
        if(pos EQUAL -1)
            message(FATAL_ERROR "Durum satirinda '${part}' yok:\n${out}")
        endif()
    endforeach()
endif()

if(DEFINED EXPECT_REGEX AND NOT EXPECT_REGEX STREQUAL "")
    string(REGEX MATCH "${EXPECT_REGEX}" matched "${out}")
    if(matched STREQUAL "")
        message(FATAL_ERROR "Durum satiri '${EXPECT_REGEX}' kalibina uymuyor:\n${out}")
    endif()
endif()

if(EXPECT_NO_PNG)
    if(EXISTS "${OUT}")
        message(FATAL_ERROR "Arguman hatasinda goruntu olusmamaliydi: ${OUT}")
    endif()
    message(STATUS "Beklenen hata kodu ${rc}: ${err}")
    return()
endif()

if(NOT EXISTS "${OUT}")
    message(FATAL_ERROR "Ekran goruntusu olusmadi: ${OUT}")
endif()

# PNG imzası ve IHDR'deki boyut (bayt 16..23, büyük uçlu)
file(READ "${OUT}" head LIMIT 24 HEX)
string(SUBSTRING "${head}" 0 16 sig)
if(NOT sig STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "PNG imzasi yanlis: ${sig}")
endif()
string(SUBSTRING "${head}" 32 8 wHex)
string(SUBSTRING "${head}" 40 8 hHex)
math(EXPR w "0x${wHex}")
math(EXPR h "0x${hHex}")
# Yüksek DPI ekranda çerçeve tamponu pencereden büyük olabilir: en az beklenen boyut
if(w LESS WIDTH OR h LESS HEIGHT)
    message(FATAL_ERROR "Goruntu boyutu ${w}x${h}, beklenen en az ${WIDTH}x${HEIGHT}")
endif()
message(STATUS "Ekran goruntusu: ${OUT} (${w}x${h})")
