#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <omp.h>

_Static_assert(sizeof(int32_t) == 4, "The canonical output requires 32-bit integers.");

// Problem variables
#define MAX_ROWS 4096
#define MAX_COLUMNS 4096
#define MAX_ITER 1000
#define X_MAX 1.0
#define X_MIN -2.0
#define Y_MAX 1.5
#define Y_MIN -1.5
#define HORSE_MAX_ITER 5000
#define HORSE_X_MAX (-0.743643887 + 1.5e-3)
#define HORSE_X_MIN (-0.743643887 - 1.5e-3)
#define HORSE_Y_MAX (0.131825904 + 1.5e-3)
#define HORSE_Y_MIN (0.131825904 - 1.5e-3)

// Output directories
#define OUTPUT_ESCAPE_BIN "output_binaries"
#define OUTPUT_IMAGES_BIN "output_images"

// Output file names
#define SERIAL_ESCAPE_FILE_NAME "serial_output.bin"
#define HORSE_SERIAL_ESCAPE_FILE_NAME "horse_serial_output.bin"
#define OPENMP_ESCAPE_FILE_NAME "openmp_output.bin"
#define HORSE_OPENMP_ESCAPE_FILE_NAME "horse_openmp_output.bin"

#define IMAGE_MONO_FILE_NAME "image_mono.ppm"
#define IMAGE_FILE_NAME "image.ppm"
#define HORSE_IMAGE_FILE_NAME "horse_image.ppm"
#define OPENMP_IMAGE_FILE_NAME "openmp_image.ppm"
#define HORSE_OPENMP_IMAGE_FILE_NAME "horse_openmp_image.ppm"

int array_alloc(int32_t** array, int rows, int columns);
int read_output(int32_t* array, size_t count, const char* file_name);

#endif
