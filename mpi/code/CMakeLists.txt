cmake_minimum_required(VERSION 4.3)
project(mandelbrot_mpi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MSMPI_ROOT "" CACHE PATH "Root of an extracted or installed MS-MPI SDK")

if(MSVC AND MSMPI_ROOT)
    add_library(mpi_c INTERFACE)
    target_include_directories(mpi_c INTERFACE
            "${MSMPI_ROOT}/Include"
            "${MSMPI_ROOT}/Include/x64")
    target_link_libraries(mpi_c INTERFACE "${MSMPI_ROOT}/Lib/x64/msmpi.lib")
else()
    find_package(MPI REQUIRED COMPONENTS C)
    add_library(mpi_c ALIAS MPI::MPI_C)
endif()

add_executable(mandelbrot_mpi
        main.c
        mandelbrot.c
        settings.c)
target_link_libraries(mandelbrot_mpi PRIVATE mpi_c)

add_executable(serial_reference
        serial_reference.c
        mandelbrot.c
        settings.c)
target_link_libraries(serial_reference PRIVATE mpi_c)
