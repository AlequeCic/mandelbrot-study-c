cmake_minimum_required(VERSION 4.3)
project(code C)

set(CMAKE_C_STANDARD 11)

add_executable(code
        main.c
        serial.c
        paralel.c
        settings.c)

find_package(OpenMP REQUIRED)
target_link_libraries(code PRIVATE OpenMP::OpenMP_C)

add_executable(benchmark
        benchmark.c
        serial.c
        paralel.c
        settings.c)
target_link_libraries(benchmark PRIVATE OpenMP::OpenMP_C)

add_executable(image
        image.c
        settings.c)

include(CTest)
if(BUILD_TESTING)
    add_executable(load_balance_test
            load_balance_test.c
            serial.c
            paralel.c
            settings.c)
    target_link_libraries(load_balance_test PRIVATE OpenMP::OpenMP_C)
    add_test(NAME load_balance_test COMMAND load_balance_test)
endif()
