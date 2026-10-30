#include "serial.h"
#include "paralel.h"
#include "../../common/load_balance.h"

#include <string.h>

#define TEST_SIDE 96
#define SCHEDULE_COUNT 3

int main(void) {
    if (fabs(calculate_load_balance_factor(8.0, 10.0) - 0.2) > 1e-12 ||
        calculate_load_balance_factor(10.0, 10.0) != 0.0 ||
        calculate_load_balance_factor(0.0, 0.0) != 0.0) {
        fprintf(stderr, "Shared load-balance formula failed.\n");
        return EXIT_FAILURE;
    }

    int32_t* reference = NULL;
    int32_t* candidate = NULL;
    if (!array_alloc(&reference, TEST_SIDE, TEST_SIDE) ||
        !array_alloc(&candidate, TEST_SIDE, TEST_SIDE)) {
        fprintf(stderr, "Unable to allocate test matrices.\n");
        free(reference);
        free(candidate);
        return EXIT_FAILURE;
    }

    calc_escape_time(reference, TEST_SIDE, TEST_SIDE, HORSE_X_MAX,
                     HORSE_X_MIN, HORSE_Y_MAX, HORSE_Y_MIN, HORSE_MAX_ITER);

    int threads = omp_get_num_procs();
    if (threads > 4) threads = 4;
    if (threads < 1) threads = 1;
    omp_set_dynamic(0);
    omp_set_num_threads(threads);

    const parallel_schedule_t schedules[SCHEDULE_COUNT] = {
        PARALLEL_SCHEDULE_STATIC,
        PARALLEL_SCHEDULE_DYNAMIC,
        PARALLEL_SCHEDULE_GUIDED
    };

    for (int index = 0; index < SCHEDULE_COUNT; index++) {
        parallel_stats_t stats;
        memset(candidate, 0, (size_t)TEST_SIDE * TEST_SIDE * sizeof(*candidate));
        calc_escape_time_parallel_scheduled(
            candidate, TEST_SIDE, TEST_SIDE, HORSE_X_MAX, HORSE_X_MIN,
            HORSE_Y_MAX, HORSE_Y_MIN, HORSE_MAX_ITER, schedules[index], 1,
            &stats);

        if (memcmp(reference, candidate,
                   (size_t)TEST_SIDE * TEST_SIDE * sizeof(*candidate)) != 0) {
            fprintf(stderr, "Parallel result differs from serial reference.\n");
            free(reference);
            free(candidate);
            return EXIT_FAILURE;
        }

        double expected = calculate_load_balance_factor(
            stats.min_thread_seconds, stats.max_thread_seconds);
        if (stats.used_threads < 1 || stats.min_thread_seconds < 0.0 ||
            stats.max_thread_seconds < stats.min_thread_seconds ||
            stats.load_balance_factor < 0.0 ||
            stats.load_balance_factor > 1.0 ||
            fabs(stats.load_balance_factor - expected) > 1e-12) {
            fprintf(stderr, "Invalid per-thread timing statistics.\n");
            free(reference);
            free(candidate);
            return EXIT_FAILURE;
        }
    }

    free(reference);
    free(candidate);
    return EXIT_SUCCESS;
}
