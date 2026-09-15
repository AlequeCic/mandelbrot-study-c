#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

//Problem variables
#define MAX_ROWS 4096
#define MAX_COLUMNS 4096
#define MAX_ITER 1000
#define X_MAX 1.0
#define X_MIN -2.0
#define Y_MAX 1.5
#define Y_MIN  -1.5

//Output bin
#define OUTPUT_BIN "output_binaries"

//file names
#define MY_ESCAPE_FILE_NAME "serial_3_multi_output"
#define TEACHER_ESCAPE_FILE_NAME "serial_output"


//general functions
//definition
void array_alloc(int16_t** array);

//implementation

void array_alloc(int16_t** array){
    *array = (int16_t*)malloc(sizeof(int16_t) * MAX_ROWS * MAX_COLUMNS);
}
