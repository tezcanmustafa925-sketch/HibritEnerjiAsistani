# data/ornek_ayar.ini, CLI'nın varsayılan ayar dökümüyle aynı mı? (cmake -P ile çalışır)
# Yeni bir ayar anahtarı eklendiğinde örnek dosyanın da yenilenmesini sağlar.
# Girdi değişkenleri: CLI (hea_cli yolu), OUT (geçici dosya), EXPECTED (depodaki dosya)

execute_process(
    COMMAND "${CLI}" --ayarlari-yaz "${OUT}"
    RESULT_VARIABLE rc
    OUTPUT_QUIET)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hea_cli --ayarlari-yaz basarisiz (cikis kodu ${rc})")
endif()
file(READ "${OUT}" generated)
file(READ "${EXPECTED}" expected)
# Satır sonu farkı (CRLF / LF) önemsiz
string(REPLACE "\r\n" "\n" generated "${generated}")
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT generated STREQUAL expected)
    message(FATAL_ERROR "data/ornek_ayar.ini eskimis. Yeniden uretmek icin:\n"
                        "  hea_cli --ayarlari-yaz data/ornek_ayar.ini")
endif()
message(STATUS "Ornek ayar dosyasi guncel")
