#include "serial.h"
#include "paralel.h"

int main(void) {
    int32_t* escape_time_array;
    const size_t pixel_count = (size_t)MAX_ROWS * (size_t)MAX_COLUMNS;

    if (!array_alloc(&escape_time_array, MAX_ROWS, MAX_COLUMNS)) {
        fprintf(stderr, "Unable to allocate the escape-time matrix.\n");
        return EXIT_FAILURE;
    }

    double start_time = omp_get_wtime();
    calc_escape_time(escape_time_array, MAX_ROWS, MAX_COLUMNS, X_MAX, X_MIN,
                     Y_MAX, Y_MIN, MAX_ITER);
    double end_time = omp_get_wtime();
    if (!write_escape_array_file(escape_time_array, pixel_count,
                                 SERIAL_ESCAPE_FILE_NAME)) {
        free(escape_time_array);
        return EXIT_FAILURE;
    }
    printf("Serial standard case: %.6f s\n", end_time - start_time);

    start_time = omp_get_wtime();
    calc_escape_time(escape_time_array, MAX_ROWS, MAX_COLUMNS, HORSE_X_MAX,
                     HORSE_X_MIN, HORSE_Y_MAX, HORSE_Y_MIN, HORSE_MAX_ITER);
    end_time = omp_get_wtime();
    if (!write_escape_array_file(escape_time_array, pixel_count,
                                 HORSE_SERIAL_ESCAPE_FILE_NAME)) {
        free(escape_time_array);
        return EXIT_FAILURE;
    }
    printf("Serial seahorse case: %.6f s\n", end_time - start_time);

    start_time = omp_get_wtime();
    calc_escape_time_parallel(escape_time_array, MAX_ROWS, MAX_COLUMNS, X_MAX,
                              X_MIN, Y_MAX, Y_MIN, MAX_ITER);
    end_time = omp_get_wtime();
    if (!write_escape_array_file(escape_time_array, pixel_count,
                                 OPENMP_ESCAPE_FILE_NAME)) {
        free(escape_time_array);
        return EXIT_FAILURE;
    }
    printf("OpenMP standard case: %.6f s\n", end_time - start_time);

    start_time = omp_get_wtime();
    calc_escape_time_parallel(escape_time_array, MAX_ROWS, MAX_COLUMNS,
                              HORSE_X_MAX, HORSE_X_MIN, HORSE_Y_MAX,
                              HORSE_Y_MIN, HORSE_MAX_ITER);
    end_time = omp_get_wtime();
    if (!write_escape_array_file(escape_time_array, pixel_count,
                                 HORSE_OPENMP_ESCAPE_FILE_NAME)) {
        free(escape_time_array);
        return EXIT_FAILURE;
    }
    printf("OpenMP seahorse case: %.6f s\n", end_time - start_time);

    free(escape_time_array);
    return EXIT_SUCCESS;
}
