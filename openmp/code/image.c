#include "settings.h"

static int write_image(const int32_t* escape_time_array, const char* file_name,
                       int max_iter) {
    const size_t pixel_count = (size_t)MAX_ROWS * (size_t)MAX_COLUMNS;
    uint8_t* image_array = malloc(pixel_count * 3 * sizeof(*image_array));
    if (image_array == NULL) {
        fprintf(stderr, "Unable to allocate the image buffer.\n");
        return 0;
    }

    for (int row = 0; row < MAX_ROWS; row++) {
        int source_row = MAX_ROWS - 1 - row;
        for (int column = 0; column < MAX_COLUMNS; column++) {
            int escape = escape_time_array[(size_t)source_row * MAX_COLUMNS + column];
            double red = 0.0;
            double green = 0.0;
            double blue = 0.0;

            if (escape != max_iter) {
                double t = (double)escape / 20.0;
                const double two_pi = 6.283185307179586;
                red = (0.5 + 0.5 * cos(two_pi * (t + 0.0))) * 255.0;
                green = (0.5 + 0.5 * cos(two_pi * (t + 0.1))) * 255.0;
                blue = (0.5 + 0.5 * cos(two_pi * (t + 0.2))) * 255.0;
            }

            size_t offset = ((size_t)row * MAX_COLUMNS + column) * 3;
            image_array[offset] = (uint8_t)red;
            image_array[offset + 1] = (uint8_t)green;
            image_array[offset + 2] = (uint8_t)blue;
        }
    }

    char output_path[256];
    snprintf(output_path, sizeof(output_path), "%s/%s", OUTPUT_IMAGES_BIN,
             file_name);
    FILE* file = fopen(output_path, "wb");
    if (file == NULL) {
        fprintf(stderr, "Unable to open %s for writing.\n", output_path);
        free(image_array);
        return 0;
    }

    fprintf(file, "P6\n%d %d\n255\n", MAX_COLUMNS, MAX_ROWS);
    size_t written = fwrite(image_array, sizeof(*image_array), pixel_count * 3,
                            file);
    int close_status = fclose(file);
    free(image_array);
    return written == pixel_count * 3 && close_status == 0;
}

int main(void) {
    const size_t pixel_count = (size_t)MAX_ROWS * (size_t)MAX_COLUMNS;
    int32_t* escape_array;
    if (!array_alloc(&escape_array, MAX_ROWS, MAX_COLUMNS)) {
        fprintf(stderr, "Unable to allocate the escape-time matrix.\n");
        return EXIT_FAILURE;
    }

    int success = read_output(escape_array, pixel_count, SERIAL_ESCAPE_FILE_NAME) &&
                  write_image(escape_array, IMAGE_FILE_NAME, MAX_ITER) &&
                  read_output(escape_array, pixel_count, OPENMP_ESCAPE_FILE_NAME) &&
                  write_image(escape_array, OPENMP_IMAGE_FILE_NAME, MAX_ITER) &&
                  read_output(escape_array, pixel_count,
                              HORSE_SERIAL_ESCAPE_FILE_NAME) &&
                  write_image(escape_array, HORSE_IMAGE_FILE_NAME,
                              HORSE_MAX_ITER) &&
                  read_output(escape_array, pixel_count,
                              HORSE_OPENMP_ESCAPE_FILE_NAME) &&
                  write_image(escape_array, HORSE_OPENMP_IMAGE_FILE_NAME,
                              HORSE_MAX_ITER);
    free(escape_array);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
