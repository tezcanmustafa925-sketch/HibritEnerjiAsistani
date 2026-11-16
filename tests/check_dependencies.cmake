# Dağıtılan exe'lerin yalnızca her Windows'ta bulunan sistem DLL'lerine bağlı olduğunu
# denetler (cmake -P). Paket başka bir bilgisayarda kurulum yapılmadan çalışacağı için
# ör. VCRUNTIME140.dll, MSVCP140.dll, ucrtbase.dll, api-ms-win-crt-*.dll ya da
# glfw3.dll'e bağımlılık hatadır. Bu DLL'ler geliştirme bilgisayarında ve GitHub'ın
# sanal makinelerinde kurulu olduğundan hata programı çalıştırarak fark edilmez:
# exe'nin içe aktarma tablosuna (dumpbin /DEPENDENTS) bakılır.
# Girdi değişkenleri:
#   DUMPBIN  dumpbin.exe yolu (MSVC ile gelir)
#   FILES    denetlenecek exe yolları (| ile ayrılmış)

# Her Windows sürümünde bulunan sistem DLL'leri (büyük harfle)
set(allowed
    KERNEL32.DLL USER32.DLL GDI32.DLL SHELL32.DLL IMM32.DLL COMDLG32.DLL
    ADVAPI32.DLL OLE32.DLL OPENGL32.DLL)

string(REPLACE "|" ";" files "${FILES}")
set(failed "")
foreach(f IN LISTS files)
    execute_process(
        COMMAND "${DUMPBIN}" /NOLOGO /DEPENDENTS "${f}"
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "dumpbin calismadi (${rc}): ${f}\n${err}${out}")
    endif()
    # Satır başındaki girintili DLL adları (normal ve gecikmeli yükleme bölümleri)
    string(REGEX MATCHALL "[\r\n][ \t]+[A-Za-z0-9_.-]+\\.[dD][lL][lL]" lines "${out}")
    if(lines STREQUAL "")
        message(FATAL_ERROR "DLL listesi okunamadi (en az KERNEL32.dll olmaliydi): ${f}\n${out}")
    endif()
    set(names "")
    foreach(line IN LISTS lines)
        string(STRIP "${line}" dll)
        list(APPEND names "${dll}")
        string(TOUPPER "${dll}" dllUpper)
        if(NOT dllUpper IN_LIST allowed)
            list(APPEND failed "${f}: ${dll}")
        endif()
    endforeach()
    message(STATUS "${f}: ${names}")
endforeach()

if(failed)
    string(REPLACE ";" "\n  " failedText "${failed}")
    message(FATAL_ERROR
        "Sistem DLL'i olmayan bagimlilik var (diger bilgisayarda calismaz):\n  ${failedText}")
endif()
