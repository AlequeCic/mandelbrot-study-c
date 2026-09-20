#include "serial.c"

int main (void){

    int16_t* escape_time_array;
    double start_time, end_time;

    array_alloc(&escape_time_array);

    //normal
    start_time = omp_get_wtime();

    calc_escape_time(escape_time_array, X_MAX, X_MIN, Y_MAX ,Y_MIN, MAX_ITER);

    end_time = omp_get_wtime();

    write_escape_array_file(escape_time_array, MAX_ROWS*MAX_COLUMNS, SERIAL_ESCAPE_FILE_NAME);

    printf("Time to calc normal: %lf\n", end_time - start_time);

    //horse
    start_time = omp_get_wtime();

    calc_escape_time(escape_time_array, HORSE_X_MAX, HORSE_X_MIN, 
        HORSE_Y_MAX, HORSE_Y_MIN, HORSE_MAX_ITER);

    end_time = omp_get_wtime();

    write_escape_array_file(escape_time_array, MAX_ROWS*MAX_COLUMNS, HORSE_SERIAL_ESCAPE_FILE_NAME);

    printf("Time to calc horse: %lf\n", end_time - start_time);


    return 0;
}