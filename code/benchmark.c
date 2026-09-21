#include "serial.h"
#include "paralel.h"

#include <limits.h>
#include <string.h>

#define REPEATS 3
#define SCHEDULE_COUNT 3
#define CHUNK_COUNT 4

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

static void build_path(char* path, size_t path_size, const char* output_dir,
                       const char* filename) {
    snprintf(path, path_size, "%s/%s", output_dir, filename);
}

static FILE* open_output(const char* output_dir, const char* filename) {
    char path[512];
    build_path(path, sizeof(path), output_dir, filename);
    FILE* file = fopen(path, "w");

    if (file == NULL) {
        fprintf(stderr, "Unable to create %s.\n", path);
        exit(EXIT_FAILURE);
    }

    return file;
}

static double run_serial(const benchmark_case_t* test_case, int16_t* output) {
    double start_time = omp_get_wtime();
    calc_escape_time(output, test_case->x_max, test_case->x_min,
                     test_case->y_max, test_case->y_min,
                     test_case->max_iter);
    return omp_get_wtime() - start_time;
}

static double run_parallel(const benchmark_case_t* test_case, int16_t* output,
                           int threads, parallel_schedule_t schedule_kind,
                           int chunk) {
    omp_set_dynamic(0);
    omp_set_num_threads(threads);

    double start_time = omp_get_wtime();
    calc_escape_time_parallel_scheduled(
        output, test_case->x_max, test_case->x_min, test_case->y_max,
        test_case->y_min, test_case->max_iter, schedule_kind, chunk, NULL);
    return omp_get_wtime() - start_time;
}

static void write_raw_serial(FILE* raw_file, const benchmark_case_t* test_case,
                             int repetition, double seconds) {
    fprintf(raw_file, "%s,serial_baseline,serial,0,1,1,%d,%.9f\n",
            test_case->name, repetition, seconds);
}

static void write_raw_parallel(FILE* raw_file,
                               const benchmark_case_t* test_case,
                               const char* suite, const char* policy, int chunk,
                               int threads, int repetition, double seconds) {
    fprintf(raw_file, "%s,%s,%s,%d,%d,%d,%d,%.9f\n", test_case->name,
            suite, policy, chunk, threads, threads, repetition, seconds);
}

static void compare_and_record(FILE* correctness_file, FILE* balance_file,
                               const benchmark_case_t* test_case,
                               const char* policy,
                               parallel_schedule_t schedule_kind,
                               int chunk, int threads,
                               const int16_t* reference, int16_t* candidate,
                               int include_balance) {
    const int total_pixels = MAX_ROWS * MAX_COLUMNS;
    const int allowed_different_pixels = total_pixels / 10000;
    int different_pixels = 0;
    int max_abs_difference = 0;
    int pixel;
    parallel_stats_t stats;

    omp_set_dynamic(0);
    omp_set_num_threads(threads);
    calc_escape_time_parallel_scheduled(
        candidate, test_case->x_max, test_case->x_min, test_case->y_max,
        test_case->y_min, test_case->max_iter, schedule_kind, chunk, &stats);

    for (pixel=0; pixel<total_pixels; pixel++) {
        int difference = abs((int)reference[pixel] - (int)candidate[pixel]);
        if (difference != 0) {
            different_pixels++;
            if (difference > max_abs_difference) {
                max_abs_difference = difference;
            }
        }
    }

    const char* status;
    if (different_pixels == 0) {
        status = "APPROVED_EXACT";
    } else if (different_pixels <= allowed_different_pixels &&
               max_abs_difference <= 1) {
        status = "APPROVED_TOLERANCE";
    } else {
        status = "REJECTED";
    }

    fprintf(correctness_file,
            "%s,%s,%d,%d,%d,%d,%d,1,%s\n",
            test_case->name, policy, chunk, threads, different_pixels,
            max_abs_difference, allowed_different_pixels, status);

    if (include_balance) {
        fprintf(balance_file,
                "%s,%s,%d,%d,%d,%lld,%lld,%lld,%.3f,%.9f\n",
                test_case->name, policy, chunk, stats.requested_threads,
                stats.used_threads, stats.total_iterations,
                stats.min_thread_iterations, stats.max_thread_iterations,
                stats.avg_thread_iterations, stats.load_balance_factor);
    }
}

static void run_serial_baseline(FILE* raw_file,
                                const benchmark_case_t* test_case,
                                int16_t* reference) {
    int repetition;

    for (repetition=1; repetition<=REPEATS; repetition++) {
        double seconds = run_serial(test_case, reference);
        write_raw_serial(raw_file, test_case, repetition, seconds);
        printf("%s serial repetition %d/%d: %.3f s\n", test_case->name,
               repetition, REPEATS, seconds);
    }
}

static void run_standard_scaling(FILE* raw_file, int16_t* output,
                                 int max_threads) {
    const int requested_threads[] = {1, 2, 4, 8, 16};
    const int count = (int)(sizeof(requested_threads) /
                            sizeof(requested_threads[0]));
    int index;

    for (index=0; index<count; index++) {
        int threads = requested_threads[index];
        int repetition;

        if (threads > max_threads) continue;

        for (repetition=1; repetition<=REPEATS; repetition++) {
            double seconds = run_parallel(&STANDARD_CASE, output, threads,
                                          PARALLEL_SCHEDULE_STATIC, 0);
            write_raw_parallel(raw_file, &STANDARD_CASE, "standard_scaling",
                               "static", 0, threads, repetition, seconds);
            printf("standard scaling static/default, %d threads, repetition "
                   "%d/%d: %.3f s\n", threads, repetition, REPEATS, seconds);
        }
    }
}

static void run_schedule_comparison(FILE* raw_file,
                                    const benchmark_case_t* test_case,
                                    int16_t* output, int threads) {
    const int chunks[CHUNK_COUNT] = {1, 4, 16, 64};
    int schedule_index;

    for (schedule_index=0; schedule_index<SCHEDULE_COUNT; schedule_index++) {
        int chunk_index;

        for (chunk_index=0; chunk_index<CHUNK_COUNT; chunk_index++) {
            int repetition;
            int chunk = chunks[chunk_index];

            for (repetition=1; repetition<=REPEATS; repetition++) {
                double seconds = run_parallel(test_case, output, threads,
                                              SCHEDULES[schedule_index].kind,
                                              chunk);
                write_raw_parallel(raw_file, test_case, "schedule_comparison",
                                   SCHEDULES[schedule_index].name, chunk,
                                   threads, repetition, seconds);
                printf("%s %s, chunk %d, %d threads, repetition %d/%d: %.3f s\n",
                       test_case->name, SCHEDULES[schedule_index].name,
                       chunk, threads, repetition, REPEATS, seconds);
            }
        }
    }
}

static void run_correctness_suite(FILE* correctness_file, FILE* balance_file,
                                  int16_t* standard_reference,
                                  int16_t* horse_reference,
                                  int16_t* candidate, int max_threads) {
    const int scaling_threads[] = {1, 2, 4, 8, 16};
    const int chunks[CHUNK_COUNT] = {1, 4, 16, 64};
    const int scaling_count = (int)(sizeof(scaling_threads) /
                                    sizeof(scaling_threads[0]));
    int index;

    for (index=0; index<scaling_count; index++) {
        int threads = scaling_threads[index];
        if (threads > max_threads) continue;
        compare_and_record(correctness_file, balance_file, &STANDARD_CASE,
                           "static", PARALLEL_SCHEDULE_STATIC, 0, threads,
                           standard_reference, candidate, 0);
    }

    for (index=0; index<SCHEDULE_COUNT; index++) {
        int chunk_index;

        for (chunk_index=0; chunk_index<CHUNK_COUNT; chunk_index++) {
            int chunk = chunks[chunk_index];
            compare_and_record(correctness_file, balance_file, &STANDARD_CASE,
                               SCHEDULES[index].name, SCHEDULES[index].kind,
                               chunk, max_threads, standard_reference,
                               candidate, 0);
            compare_and_record(correctness_file, balance_file, &HORSE_CASE,
                               SCHEDULES[index].name, SCHEDULES[index].kind,
                               chunk, max_threads, horse_reference, candidate,
                               1);
        }
    }
}

int main(int argc, char* argv[]) {
    const char* output_dir;
    int max_threads;
    int16_t* standard_reference;
    int16_t* horse_reference;
    int16_t* output;
    FILE* raw_file;
    FILE* correctness_file;
    FILE* balance_file;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <existing-output-directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    output_dir = argv[1];
    omp_set_dynamic(0);
    max_threads = omp_get_num_procs();
    if (max_threads < 1) max_threads = 1;
    if (max_threads > 16) max_threads = 16;

    array_alloc(&standard_reference);
    array_alloc(&horse_reference);
    array_alloc(&output);
    if (standard_reference == NULL || horse_reference == NULL || output == NULL) {
        fprintf(stderr, "Unable to allocate Mandelbrot matrices.\n");
        free(standard_reference);
        free(horse_reference);
        free(output);
        return EXIT_FAILURE;
    }

    raw_file = open_output(output_dir, "raw_timings.csv");
    correctness_file = open_output(output_dir, "correctness.csv");
    balance_file = open_output(output_dir, "seahorse_load_balance.csv");

    fprintf(raw_file,
            "case,suite,policy,chunk,requested_threads,used_threads,rep,seconds\n");
    fprintf(correctness_file,
            "case,policy,chunk,threads,different_pixels,max_abs_difference,allowed_different_pixels,allowed_max_abs_difference,status\n");
    fprintf(balance_file,
            "case,policy,chunk,requested_threads,used_threads,total_iterations,min_thread_iterations,max_thread_iterations,avg_thread_iterations,load_balance_factor\n");

    printf("OpenMP benchmark using up to %d threads and %d repetitions.\n",
           max_threads, REPEATS);
    run_serial_baseline(raw_file, &STANDARD_CASE, standard_reference);
    run_serial_baseline(raw_file, &HORSE_CASE, horse_reference);
    run_standard_scaling(raw_file, output, max_threads);
    run_schedule_comparison(raw_file, &STANDARD_CASE, output, max_threads);
    run_schedule_comparison(raw_file, &HORSE_CASE, output, max_threads);
    run_correctness_suite(correctness_file, balance_file, standard_reference,
                          horse_reference, output, max_threads);

    fclose(raw_file);
    fclose(correctness_file);
    fclose(balance_file);
    free(standard_reference);
    free(horse_reference);
    free(output);

    printf("Benchmark completed successfully.\n");
    return EXIT_SUCCESS;
}
