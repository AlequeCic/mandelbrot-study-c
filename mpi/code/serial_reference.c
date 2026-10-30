#include "settings.h"

#include <time.h>

static double wall_time(void) {
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) {
        return 0.0;
    }
    return (double)value.tv_sec + (double)value.tv_nsec / 1.0e9;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
                "Use: %s <standard|seahorse> [tamanho=4096] [saida.bin|-]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    int is_horse = strcmp(argv[1], "seahorse") == 0;
    int is_standard = strcmp(argv[1], "standard") == 0;
    int size = argc > 2 ? atoi(argv[2]) : MAX_ROWS;
    const char* default_name = is_horse ? "horse_serial_reference.bin"
                                        : "serial_reference.bin";
    const char* output_file = argc > 3 ? argv[3] : default_name;

    if (!(is_horse || is_standard) || size <= 0) {
        fprintf(stderr, "Caso ou tamanho invalido.\n");
        return EXIT_FAILURE;
    }

    int32_t* image = NULL;
    if (!array_alloc(&image, size, size)) {
        fprintf(stderr, "Falha de alocacao da matriz serial.\n");
        return EXIT_FAILURE;
    }

    double start = wall_time();
    for (int row = 0; row < size; row++) {
        calc_row(image + (size_t)row * size, row, size, is_horse);
    }
    double compute_time = wall_time() - start;

    double write_time = 0.0;
    int output_written = 0;
    if (strcmp(output_file, "-") != 0) {
        double write_start = wall_time();
        output_written = write_output(image, (size_t)size * size, output_file);
        write_time = wall_time() - write_start;
    }

    printf("serial_summary case=%s size=%d compute=%.9f write=%.9f output_written=%d\n",
           argv[1], size, compute_time, write_time, output_written);
    free(image);
    return output_written || strcmp(output_file, "-") == 0
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
