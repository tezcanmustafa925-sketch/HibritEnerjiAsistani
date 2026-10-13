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
