# Terminal uygulaması: core'dan gelen StepResult verilerini ekrana ve CSV'ye yazar.
add_executable(hea_cli
    main.cpp
    csv_export.cpp
    report.cpp
    tables.cpp
)

target_link_libraries(hea_cli PRIVATE hea_core)

if(MSVC)
    # fopen gibi standart C fonksiyonları için MSVC'nin "güvensiz" uyarısını kapat
    target_compile_definitions(hea_cli PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
