#ifndef PARALEL_H
#define PARALEL_H

#include "settings.h"

typedef struct {
    int requested_threads;
    int used_threads;
    long long total_iterations;
    long long max_thread_iterations;
    long long min_thread_iterations;
    double avg_thread_iterations;
    double load_balance_factor;
} parallel_stats_t;

typedef enum {
    PARALLEL_SCHEDULE_STATIC,
    PARALLEL_SCHEDULE_DYNAMIC,
    PARALLEL_SCHEDULE_GUIDED
} parallel_schedule_t;

void calc_escape_time_parallel(int32_t* array, int rows, int columns,
                               double x_max, double x_min, double y_max,
                               double y_min, int max_iter);
void calc_escape_time_parallel_scheduled(int32_t* array, int rows,
                                         int columns, double x_max,
                                         double x_min, double y_max,
                                         double y_min, int max_iter,
                                         parallel_schedule_t schedule_kind,
                                         int chunk,
                                         parallel_stats_t* stats);

#endif
