/*
This file has the parallel functions.
*/

#include "paralel.h"
#include <limits.h>

typedef struct {
    long long iterations;
    int rows;
    char padding[64 - sizeof(long long) - sizeof(int)];
} thread_load_t;

static inline int calculate_escape_time(double pos_x, double pos_y,
                                        int max_iter) {
    double z_real = 0;
    double z_imaginary = 0;
    int i;

    for (i=0; i<max_iter; i++) {
        double temp_z_real = z_real*z_real -
            (z_imaginary * z_imaginary) + pos_x;
        double z_distance;

        z_imaginary = pos_y + 2.0*z_real*z_imaginary;
        z_real = temp_z_real;
        z_distance = z_real*z_real + z_imaginary*z_imaginary;
        if (z_distance > 4.0) break;
    }

    return i;
}

#define PROCESS_ROWS_WITHOUT_STATS(OMP_FOR) \
    OMP_FOR \
    for (py=0; py<MAX_ROWS; py++) { \
        double pos_y = y_min + (py * y_step_size); \
        int px; \
        for (px=0; px<MAX_COLUMNS; px++) { \
            double pos_x = x_min + (px * x_step_size); \
            int i = calculate_escape_time(pos_x, pos_y, max_iter); \
            array[MAX_COLUMNS*py + px] = (int16_t)i; \
        } \
    }

#define PROCESS_ROWS_WITH_STATS(OMP_FOR) \
    OMP_FOR \
    for (py=0; py<MAX_ROWS; py++) { \
        double pos_y = y_min + (py * y_step_size); \
        int px; \
        for (px=0; px<MAX_COLUMNS; px++) { \
            double pos_x = x_min + (px * x_step_size); \
            int i = calculate_escape_time(pos_x, pos_y, max_iter); \
            array[MAX_COLUMNS*py + px] = (int16_t)i; \
            thread_loads[thread_id].iterations += i; \
        } \
        thread_loads[thread_id].rows++; \
    }

#define OMP_FOR_STATIC _Pragma("omp for schedule(static, chunk)")
#define OMP_FOR_STATIC_DEFAULT _Pragma("omp for schedule(static)")
#define OMP_FOR_DYNAMIC _Pragma("omp for schedule(dynamic, chunk)")
#define OMP_FOR_GUIDED _Pragma("omp for schedule(guided, chunk)")
#define OMP_PARALLEL_FOR_STATIC _Pragma("omp parallel for schedule(static, chunk)")
#define OMP_PARALLEL_FOR_STATIC_DEFAULT _Pragma("omp parallel for schedule(static)")
#define OMP_PARALLEL_FOR_DYNAMIC _Pragma("omp parallel for schedule(dynamic, chunk)")
#define OMP_PARALLEL_FOR_GUIDED _Pragma("omp parallel for schedule(guided, chunk)")

static void calc_escape_time_parallel_without_stats(
    int16_t* array, double x_max, double x_min, double y_max,
    double y_min, int max_iter, parallel_schedule_t schedule_kind,
    int chunk) {
    double x_step_size = (x_max - x_min)/MAX_COLUMNS;
    double y_step_size = (y_max - y_min)/MAX_ROWS;
    int py;

    if (schedule_kind == PARALLEL_SCHEDULE_STATIC && chunk == 0) {
        PROCESS_ROWS_WITHOUT_STATS(OMP_PARALLEL_FOR_STATIC_DEFAULT);
    } else if (schedule_kind == PARALLEL_SCHEDULE_STATIC) {
        PROCESS_ROWS_WITHOUT_STATS(OMP_PARALLEL_FOR_STATIC);
    } else if (schedule_kind == PARALLEL_SCHEDULE_DYNAMIC) {
        PROCESS_ROWS_WITHOUT_STATS(OMP_PARALLEL_FOR_DYNAMIC);
    } else {
        PROCESS_ROWS_WITHOUT_STATS(OMP_PARALLEL_FOR_GUIDED);
    }
}

void calc_escape_time_parallel_scheduled(
    int16_t* array, double x_max, double x_min, double y_max,
    double y_min, int max_iter, parallel_schedule_t schedule_kind, int chunk,
    parallel_stats_t* stats) {

    if (chunk < 1 && schedule_kind != PARALLEL_SCHEDULE_STATIC) chunk = 1;

    if (stats == NULL) {
        calc_escape_time_parallel_without_stats(array, x_max, x_min, y_max,
                                                y_min, max_iter, schedule_kind,
                                                chunk);
        return;
    }

    int max_threads = omp_get_max_threads();
    thread_load_t* thread_loads = calloc((size_t)max_threads,
                                         sizeof(*thread_loads));

    if (thread_loads == NULL) {
        free(thread_loads);
        fprintf(stderr, "Unable to allocate per-thread statistics.\n");
        exit(EXIT_FAILURE);
    }

    double x_step_size = (x_max - x_min)/MAX_COLUMNS;
    double y_step_size = (y_max - y_min)/MAX_ROWS;
    int used_threads = 0;
    int py;

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();

        #pragma omp single
        used_threads = omp_get_num_threads();

        if (schedule_kind == PARALLEL_SCHEDULE_STATIC && chunk == 0) {
            PROCESS_ROWS_WITH_STATS(OMP_FOR_STATIC_DEFAULT);
        } else if (schedule_kind == PARALLEL_SCHEDULE_STATIC) {
            PROCESS_ROWS_WITH_STATS(OMP_FOR_STATIC);
        } else if (schedule_kind == PARALLEL_SCHEDULE_DYNAMIC) {
            PROCESS_ROWS_WITH_STATS(OMP_FOR_DYNAMIC);
        } else {
            PROCESS_ROWS_WITH_STATS(OMP_FOR_GUIDED);
        }
    }

    long long total_iterations = 0;
    long long max_thread_iterations = 0;
    long long min_thread_iterations = LLONG_MAX;
    int thread_id;

    for (thread_id=0; thread_id<used_threads; thread_id++) {
        long long load = thread_loads[thread_id].iterations;
        total_iterations += load;
        if (load > max_thread_iterations) max_thread_iterations = load;
        if (load < min_thread_iterations) min_thread_iterations = load;
    }

    stats->requested_threads = omp_get_max_threads();
    stats->used_threads = used_threads;
    stats->total_iterations = total_iterations;
    stats->max_thread_iterations = max_thread_iterations;
    stats->min_thread_iterations = min_thread_iterations;
    stats->avg_thread_iterations = (double)total_iterations / used_threads;
    stats->load_balance_factor =
        max_thread_iterations / stats->avg_thread_iterations;

    free(thread_loads);
}

void calc_escape_time_parallel(int16_t* array, double x_max, double x_min,
                               double y_max, double y_min, int max_iter) {
    calc_escape_time_parallel_scheduled(array, x_max, x_min, y_max, y_min,
                                        max_iter, PARALLEL_SCHEDULE_STATIC, 0,
                                        NULL);
}
