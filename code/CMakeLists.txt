cmake_minimum_required(VERSION 4.3)
project(code C)

set(CMAKE_C_STANDARD 11)

add_executable(code
        error_check.c
        main.c
        serial.c
        settings.c)
