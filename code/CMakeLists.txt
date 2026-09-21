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
