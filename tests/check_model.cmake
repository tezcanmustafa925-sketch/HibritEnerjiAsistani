# Uçtan uca model eğitimi testi (cmake -P ile çalışır):
#   1) hea_cli ile eğitim ve test verisini üret
#   2) model/egit.py ile modeli eğit, --kontrol ile doğruluk eşiklerini denetle
#   3) Yeni modeli depodaki modelle (REFERENCE) karşılaştır: fizik değişip
#      katsayılar eskidiyse test başarısız olur
# Girdi değişkenleri: CLI, PYTHON, SCRIPT, WORKDIR, REFERENCE

file(MAKE_DIRECTORY "${WORKDIR}")
foreach(kume egitim test)
    execute_process(
        COMMAND "${CLI}" --cevrim ${kume} --egitim-verisi "${WORKDIR}/${kume}.csv"
        RESULT_VARIABLE rc OUTPUT_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "hea_cli ${kume} verisini uretemedi (cikis kodu ${rc})")
    endif()
endforeach()

execute_process(
    COMMAND "${PYTHON}" "${SCRIPT}"
            --egitim "${WORKDIR}/egitim.csv" --test "${WORKDIR}/test.csv"
            --cikti "${WORKDIR}/tuketim_modeli.json" --grafiksiz --kontrol
            --referans "${REFERENCE}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "egit.py basarisiz (cikis kodu ${rc}):\n${out}\n${err}")
endif()
message(STATUS "${out}")
message(STATUS "Model egitimi ve kontrolu basarili")
