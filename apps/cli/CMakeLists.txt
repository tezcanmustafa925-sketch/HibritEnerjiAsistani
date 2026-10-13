# Terminal uygulaması: core'dan gelen StepResult verilerini ekrana yazar.
add_executable(hea_cli
    main.cpp
)

target_link_libraries(hea_cli PRIVATE hea_core)
