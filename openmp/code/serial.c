/* Serial Mandelbrot escape-time implementation and canonical binary output. */

#include "serial.h"

int write_escape_array_file(const int32_t* pointer, size_t count,
                            const char* output_name) {
    char output_path[256];
    snprintf(output_path, sizeof(output_path), "%s/%s", OUTPUT_ESCAPE_BIN,
             output_name);

    FILE* file = fopen(output_path, "wb");
    if (file == NULL) {
        fprintf(stderr, "Unable to open %s for writing.\n", output_path);
        return 0;
    }

    size_t written = fwrite(pointer, sizeof(*pointer), count, file);
    int close_status = fclose(file);
    if (written != count || close_status != 0) {
        fprintf(stderr, "Unable to write the complete matrix to %s.\n",
                output_path);
        return 0;
    }
    return 1;
}

void calc_escape_time(int32_t* array, int rows, int columns,
                      double x_max, double x_min, double y_max,
                      double y_min, int max_iter) {
    double x_step_size = (x_max - x_min) / columns;
    double y_step_size = (y_max - y_min) / rows;

    for (int py = 0; py < rows; py++) {
        double pos_y = y_min + py * y_step_size;

        for (int px = 0; px < columns; px++) {
            double pos_x = x_min + px * x_step_size;
            double z_real = 0.0;
            double z_imaginary = 0.0;
            int iteration;

            for (iteration = 0; iteration < max_iter; iteration++) {
                double next_z_real = z_real * z_real -
                    z_imaginary * z_imaginary + pos_x;
                z_imaginary = pos_y + 2.0 * z_real * z_imaginary;
                z_real = next_z_real;

                if (z_real * z_real + z_imaginary * z_imaginary > 4.0) {
                    break;
                }
            }
            array[(size_t)py * (size_t)columns + (size_t)px] = iteration;
        }
    }
}
