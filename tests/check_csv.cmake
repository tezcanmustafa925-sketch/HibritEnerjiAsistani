# CLI'nın CSV çıktısını kontrol eden betik (cmake -P ile çalışır).
# Girdi değişkenleri: CLI (hea_cli yolu), OUT (CSV yolu), EXPECTED_ROWS (veri satırı sayısı)

file(REMOVE "${OUT}")
execute_process(
    COMMAND "${CLI}" --cevrim sehir_101 --strateji kural --csv "${OUT}"
    RESULT_VARIABLE rc
    OUTPUT_QUIET)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hea_cli basarisiz oldu (cikis kodu ${rc})")
endif()
if(NOT EXISTS "${OUT}")
    message(FATAL_ERROR "CSV dosyasi olusmadi: ${OUT}")
endif()

file(STRINGS "${OUT}" lines)
list(LENGTH lines n)
math(EXPR expected "${EXPECTED_ROWS} + 1")
if(NOT n EQUAL expected)
    message(FATAL_ERROR "Satir sayisi ${n}, beklenen ${expected} (baslik + ${EXPECTED_ROWS})")
endif()

# Başlık ve ilk veri satırının sütun sayıları eşit olmalı
list(GET lines 0 header)
list(GET lines 1 first)
if(NOT header MATCHES "^cycle,strategy,time_s,")
    message(FATAL_ERROR "Beklenmeyen baslik: ${header}")
endif()
string(REPLACE "," ";" headerFields "${header}")
string(REPLACE "," ";" firstFields "${first}")
list(LENGTH headerFields nh)
list(LENGTH firstFields nf)
if(NOT nh EQUAL nf)
    message(FATAL_ERROR "Sutun sayisi uyusmuyor: baslik ${nh}, veri ${nf}")
endif()
if(NOT first MATCHES "^sehir_101,kural-tabanli,1,1,")
    message(FATAL_ERROR "Beklenmeyen ilk satir: ${first}")
endif()
message(STATUS "CSV dogru: ${n} satir, ${nh} sutun")
