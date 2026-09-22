#include "serial.h"
#include "paralel.h"

#include <limits.h>
#include <string.h>

#define REPEATS 3
#define SCHEDULE_COUNT 3
#define CHUNK_COUNT 4
#define WEAK_SCALING_COUNT 3

typedef struct {
    const char* name;
    double x_max;
    double x_min;
    double y_max;
    double y_min;
    int max_iter;
} benchmark_case_t;

typedef struct {
    const char* name;
    parallel_schedule_t kind;
} schedule_config_t;

typedef struct {
    int threads;
    int side;
} weak_scaling_config_t;

typedef struct {
    int tested;
    int exact;
    int within_tolerance;
    int rejected;
} correctness_summary_t;

static const benchmark_case_t STANDARD_CASE = {
    "standard", X_MAX, X_MIN, Y_MAX, Y_MIN, MAX_ITER
};

static const benchmark_case_t HORSE_CASE = {
    "seahorse_valley", HORSE_X_MAX, HORSE_X_MIN,
    HORSE_Y_MAX, HORSE_Y_MIN, HORSE_MAX_ITER
};

static const schedule_config_t SCHEDULES[SCHEDULE_COUNT] = {
    {"static", PARALLEL_SCHEDULE_STATIC},
    {"dynamic", PARALLEL_SCHEDULE_DYNAMIC},
    {"guided", PARALLEL_SCHEDULE_GUIDED}
};

/* Each configuration keeps 4096^2 pixels per OpenMP thread. */
static const weak_scaling_config_t WEAK_SCALING[WEAK_SCALING_COUNT] = {
    {1, 4096},
    {4, 8192},
    {16, 16384}
};

static FILE* open_output(const char* output_dir, const char* filename) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", output_dir, filename);
    FILE* file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "Unable to create %s.\n", path);
        exit(EXIT_FAILURE);
    }
    return file;
}

static double run_serial(const benchmark_case_t* test_case, int32_t* output,
                         int rows, int columns) {
    double start_time = omp_get_wtime();
    calc_escape_time(output, rows, columns, test_case->x_max, test_case->x_min,
                     test_case->y_max, test_case->y_min, test_case->max_iter);
    return omp_get_wtime() - start_time;
}

static double run_parallel(const benchmark_case_t* test_case, int32_t* output,
                           int rows, int columns, int threads,
                           parallel_schedule_t schedule_kind, int chunk) {
    omp_set_dynamic(0);
    omp_set_num_threads(threads);

    double start_time = omp_get_wtime();
    calc_escape_time_parallel_scheduled(
        output, rows, columns, test_case->x_max, test_case->x_min,
        test_case->y_max, test_case->y_min, test_case->max_iter,
        schedule_kind, chunk, NULL);
    return omp_get_wtime() - start_time;
}

static void write_raw_serial(FILE* raw_file, const benchmark_case_t* test_case,
                             int rows, int columns, int repetition,
                             double seconds) {
    fprintf(raw_file,
            "%s,serial_baseline,serial,0,1,1,%d,%d,%zu,%d,%.9f\n",
            test_case->name, columns, rows, (size_t)rows * (size_t)columns,
            repetition, seconds);
}

static void write_raw_parallel(FILE* raw_file,
                               const benchmark_case_t* test_case,
                               const char* suite, const char* policy, int chunk,
                               int rows, int columns, int threads,
                               int repetition, double seconds) {
    fprintf(raw_file, "%s,%s,%s,%d,%d,%d,%d,%d,%zu,%d,%.9f\n",
            test_case->name, suite, policy, chunk, threads, threads, columns,
            rows, (size_t)rows * (size_t)columns, repetition, seconds);
}

static int compare_and_record(FILE* correctness_file, FILE* balance_file,
                               const benchmark_case_t* test_case,
                               const char* policy,
                               parallel_schedule_t schedule_kind, int chunk,
                               int threads, int rows, int columns,
                               const int32_t* reference, int32_t* candidate,
                               int include_balance) {
    const size_t total_pixels = (size_t)rows * (size_t)columns;
    const size_t allowed_different_pixels = total_pixels / 10000;
    size_t different_pixels = 0;
    int max_abs_difference = 0;
    parallel_stats_t stats;

    omp_set_dynamic(0);
    omp_set_num_threads(threads);
    calc_escape_time_parallel_scheduled(
        candidate, rows, columns, test_case->x_max, test_case->x_min,
        test_case->y_max, test_case->y_min, test_case->max_iter,
        schedule_kind, chunk, &stats);

    for (size_t pixel = 0; pixel < total_pixels; pixel++) {
        int difference = abs(reference[pixel] - candidate[pixel]);
        if (difference != 0) {
            different_pixels++;
            if (difference > max_abs_difference) {
                max_abs_difference = difference;
            }
        }
    }

    const char* status;
    int result;
    if (stats.used_threads != threads) {
        status = "REJECTED_THREAD_COUNT";
        result = 2;
    } else if (different_pixels == 0) {
        status = "APPROVED_EXACT";
        result = 0;
    } else if (different_pixels <= allowed_different_pixels &&
               max_abs_difference <= 1) {
        status = "APPROVED_TOLERANCE";
        result = 1;
    } else {
        status = "REJECTED";
        result = 2;
    }

    fprintf(correctness_file, "%s,%s,%d,%d,%d,%zu,%d,%zu,1,%s\n",
            test_case->name, policy, chunk, threads, stats.used_threads,
            different_pixels,
            max_abs_difference, allowed_different_pixels, status);
    printf("correctness: %s %s chunk=%d threads=%d used=%d: %s, %zu differing pixels\n",
           test_case->name, policy, chunk, threads, stats.used_threads,
           status, different_pixels);

    if (include_balance) {
        fprintf(balance_file,
                "%s,%s,%d,%d,%d,%lld,%lld,%lld,%.3f,%.9f\n",
                test_case->name, policy, chunk, stats.requested_threads,
                stats.used_threads, stats.total_iterations,
                stats.min_thread_iterations, stats.max_thread_iterations,
                stats.avg_thread_iterations, stats.load_balance_factor);
    }
    return result;
}

static void tally_result(correctness_summary_t* summary, int result) {
    summary->tested++;
    if (result == 0) summary->exact++;
    else if (result == 1) summary->within_tolerance++;
    else summary->rejected++;
}

static void run_serial_baseline(FILE* raw_file,
                                const benchmark_case_t* test_case,
                                int32_t* reference) {
    for (int repetition = 1; repetition <= REPEATS; repetition++) {
        double seconds = run_serial(test_case, reference, MAX_ROWS, MAX_COLUMNS);
        write_raw_serial(raw_file, test_case, MAX_ROWS, MAX_COLUMNS, repetition,
                         seconds);
        printf("%s serial %d/%d: %.3f s\n", test_case->name, repetition,
               REPEATS, seconds);
    }
}

static void run_standard_scaling(FILE* raw_file, int32_t* output,
                                 int max_threads) {
    const int requested_threads[] = {1, 2, 4, 8, 16};
    const int count = (int)(sizeof(requested_threads) /
                            sizeof(requested_threads[0]));

    for (int index = 0; index < count; index++) {
        int threads = requested_threads[index];
        if (threads > max_threads) {
            continue;
        }
        for (int repetition = 1; repetition <= REPEATS; repetition++) {
            double seconds = run_parallel(&STANDARD_CASE, output, MAX_ROWS,
                                          MAX_COLUMNS, threads,
                                          PARALLEL_SCHEDULE_STATIC, 0);
            write_raw_parallel(raw_file, &STANDARD_CASE, "strong_scaling",
                               "static", 0, MAX_ROWS, MAX_COLUMNS, threads,
                               repetition, seconds);
            printf("strong scaling, %d threads, %d/%d: %.3f s\n", threads,
                   repetition, REPEATS, seconds);
        }
    }
}

static void run_weak_scaling(FILE* weak_file, int max_threads) {
    for (int index = 0; index < WEAK_SCALING_COUNT; index++) {
        const weak_scaling_config_t config = WEAK_SCALING[index];
        if (config.threads > max_threads) {
            continue;
        }

        int32_t* output;
        if (!array_alloc(&output, config.side, config.side)) {
            fprintf(stderr, "Unable to allocate %dx%d weak-scaling matrix.\n",
                    config.side, config.side);
            exit(EXIT_FAILURE);
        }

        for (int repetition = 1; repetition <= REPEATS; repetition++) {
            double seconds = run_parallel(&STANDARD_CASE, output, config.side,
                                          config.side, config.threads,
                                          PARALLEL_SCHEDULE_STATIC, 0);
            fprintf(weak_file, "%d,%d,%d,%zu,static,0,%d,%.9f\n",
                    config.threads, config.side, config.side,
                    (size_t)config.side * (size_t)config.side, repetition,
                    seconds);
            printf("weak scaling, %d threads, %dx%d, %d/%d: %.3f s\n",
                   config.threads, config.side, config.side, repetition,
                   REPEATS, seconds);
        }
        free(output);
    }
}

static void run_schedule_comparison(FILE* raw_file,
                                    const benchmark_case_t* test_case,
                                    int32_t* output, int threads) {
    const int chunks[CHUNK_COUNT] = {1, 4, 16, 64};
    for (int schedule_index = 0; schedule_index < SCHEDULE_COUNT;
         schedule_index++) {
        for (int chunk_index = 0; chunk_index < CHUNK_COUNT; chunk_index++) {
            int chunk = chunks[chunk_index];
            for (int repetition = 1; repetition <= REPEATS; repetition++) {
                double seconds = run_parallel(test_case, output, MAX_ROWS,
                                              MAX_COLUMNS, threads,
                                              SCHEDULES[schedule_index].kind,
                                              chunk);
                write_raw_parallel(raw_file, test_case, "schedule_comparison",
                                   SCHEDULES[schedule_index].name, chunk,
                                   MAX_ROWS, MAX_COLUMNS, threads, repetition,
                                   seconds);
                printf("%s %s, chunk %d, %d/%d: %.3f s\n", test_case->name,
                       SCHEDULES[schedule_index].name, chunk, repetition,
                       REPEATS, seconds);
            }
        }
    }
}

static correctness_summary_t run_correctness_suite(
    FILE* correctness_file, FILE* balance_file,
    int32_t* standard_reference, int32_t* horse_reference,
    int32_t* candidate, int max_threads) {
    correctness_summary_t summary = {0};
    const int scaling_threads[] = {1, 2, 4, 8, 16};
    const int chunks[CHUNK_COUNT] = {1, 4, 16, 64};
    const int scaling_count = (int)(sizeof(scaling_threads) /
                                    sizeof(scaling_threads[0]));

    for (int index = 0; index < scaling_count; index++) {
        int threads = scaling_threads[index];
        if (threads > max_threads) {
            continue;
        }
        tally_result(&summary, compare_and_record(
            correctness_file, balance_file, &STANDARD_CASE,
            "static", PARALLEL_SCHEDULE_STATIC, 0, threads,
            MAX_ROWS, MAX_COLUMNS, standard_reference, candidate, 0));
    }

    for (int index = 0; index < SCHEDULE_COUNT; index++) {
        for (int chunk_index = 0; chunk_index < CHUNK_COUNT; chunk_index++) {
            int chunk = chunks[chunk_index];
            tally_result(&summary, compare_and_record(
                correctness_file, balance_file, &STANDARD_CASE,
                SCHEDULES[index].name, SCHEDULES[index].kind,
                chunk, max_threads, MAX_ROWS, MAX_COLUMNS,
                standard_reference, candidate, 0));
            tally_result(&summary, compare_and_record(
                correctness_file, balance_file, &HORSE_CASE,
                SCHEDULES[index].name, SCHEDULES[index].kind,
                chunk, max_threads, MAX_ROWS, MAX_COLUMNS,
                horse_reference, candidate, 1));
        }
    }
    return summary;
}

int main(int argc, char* argv[]) {
    int correctness_only = argc == 3 &&
                           strcmp(argv[1], "--correctness-only") == 0;
    if (!correctness_only && argc != 2) {
        fprintf(stderr, "Usage: %s [--correctness-only] <existing-output-directory>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char* output_dir = argv[correctness_only ? 2 : 1];
    omp_set_dynamic(0);
    int max_threads = omp_get_num_procs();
    if (max_threads < 1) {
        max_threads = 1;
    }
    if (max_threads > 16) {
        max_threads = 16;
    }

    int32_t* standard_reference = NULL;
    int32_t* horse_reference = NULL;
    int32_t* output = NULL;
    if (!array_alloc(&standard_reference, MAX_ROWS, MAX_COLUMNS) ||
        !array_alloc(&horse_reference, MAX_ROWS, MAX_COLUMNS) ||
        !array_alloc(&output, MAX_ROWS, MAX_COLUMNS)) {
        fprintf(stderr, "Unable to allocate Mandelbrot matrices.\n");
        free(standard_reference);
        free(horse_reference);
        free(output);
        return EXIT_FAILURE;
    }

    FILE* raw_file = correctness_only ? NULL :
                     open_output(output_dir, "raw_timings.csv");
    FILE* correctness_file = open_output(output_dir, "correctness.csv");
    FILE* balance_file = open_output(output_dir, "seahorse_load_balance.csv");
    FILE* weak_file = correctness_only ? NULL :
                      open_output(output_dir, "weak_scaling.csv");

    if (raw_file != NULL) {
        fprintf(raw_file,
                "case,suite,policy,chunk,requested_threads,used_threads,width,height,pixels,rep,seconds\n");
    }
    fprintf(correctness_file,
            "case,policy,chunk,threads,used_threads,different_pixels,max_abs_difference,allowed_different_pixels,allowed_max_abs_difference,status\n");
    fprintf(balance_file,
            "case,policy,chunk,requested_threads,used_threads,total_iterations,min_thread_iterations,max_thread_iterations,avg_thread_iterations,load_balance_factor\n");
    if (weak_file != NULL) {
        fprintf(weak_file,
                "threads,width,height,pixels,policy,chunk,rep,seconds\n");
    }

    if (correctness_only) {
        calc_escape_time(standard_reference, MAX_ROWS, MAX_COLUMNS,
                         X_MAX, X_MIN, Y_MAX, Y_MIN, MAX_ITER);
        calc_escape_time(horse_reference, MAX_ROWS, MAX_COLUMNS,
                         HORSE_X_MAX, HORSE_X_MIN, HORSE_Y_MAX,
                         HORSE_Y_MIN, HORSE_MAX_ITER);
    } else {
        printf("OpenMP benchmark using up to %d threads and %d repetitions.\n",
               max_threads, REPEATS);
        run_serial_baseline(raw_file, &STANDARD_CASE, standard_reference);
        run_serial_baseline(raw_file, &HORSE_CASE, horse_reference);
        run_standard_scaling(raw_file, output, max_threads);
        run_schedule_comparison(raw_file, &STANDARD_CASE, output, max_threads);
        run_schedule_comparison(raw_file, &HORSE_CASE, output, max_threads);
    }
    correctness_summary_t summary = run_correctness_suite(
        correctness_file, balance_file, standard_reference,
        horse_reference, output, max_threads);
    if (!correctness_only && summary.rejected == 0 &&
        summary.within_tolerance == 0) {
        run_weak_scaling(weak_file, max_threads);
    }

    int write_failed = 0;
    if (raw_file != NULL && ferror(raw_file)) write_failed = 1;
    if (ferror(correctness_file) || ferror(balance_file)) write_failed = 1;
    if (weak_file != NULL && ferror(weak_file)) write_failed = 1;
    if (raw_file != NULL && fclose(raw_file) != 0) write_failed = 1;
    if (fclose(correctness_file) != 0) write_failed = 1;
    if (fclose(balance_file) != 0) write_failed = 1;
    if (weak_file != NULL && fclose(weak_file) != 0) write_failed = 1;
    free(standard_reference);
    free(horse_reference);
    free(output);
    printf("Correctness: %d tested, %d exact, %d within tolerance, %d rejected.\n",
           summary.tested, summary.exact, summary.within_tolerance,
           summary.rejected);
    if (summary.rejected != 0 || summary.within_tolerance != 0 ||
        write_failed) {
        fprintf(stderr, "Benchmark failed correctness or output checks.\n");
        return EXIT_FAILURE;
    }
    printf("Benchmark completed successfully.\n");
    return EXIT_SUCCESS;
}
