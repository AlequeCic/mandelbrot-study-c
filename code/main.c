#include "serial.c"

int main (void){

    int16_t* escape_time_array;
    double start_time, end_time;

    array_alloc(&escape_time_array);

    start_time = omp_get_wtime();

    calc_escape_time_3_multi(escape_time_array);

    end_time = omp_get_wtime();

    write_escape_array_file(escape_time_array, MAX_ROWS*MAX_COLUMNS, "serial_3_multi_output");

    printf("Time to calc 3 mult: %lf\n", end_time - start_time);

    start_time = omp_get_wtime();

    calc_escape_time(escape_time_array);

    end_time = omp_get_wtime();

    write_escape_array_file(escape_time_array, MAX_ROWS*MAX_COLUMNS, "serial_output");

    printf("Time to calc normal: %lf\n", end_time - start_time);


    return 0;
}