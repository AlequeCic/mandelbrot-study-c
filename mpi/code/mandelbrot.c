#include "settings.h"

void calc_row(int32_t* out_row, int py, int size, int is_horse){
    int max_iter = is_horse? HORSE_MAX_ITER: MAX_ITER;
    double x_min = is_horse ? HORSE_X_MIN : X_MIN;
    double x_max = is_horse ? HORSE_X_MAX : X_MAX;
    double y_min = is_horse ? HORSE_Y_MIN : Y_MIN;
    double y_max = is_horse ? HORSE_Y_MAX : Y_MAX;

    double x_step = (x_max - x_min) / size;
    double y_step = (y_max - y_min) / size;
    double pos_y = y_min + py * y_step;

    for (int px = 0; px < size; px++) {
        double pos_x = x_min + px * x_step;
        double z_real = 0.0;
        double z_imaginary = 0.0;
        int iteration;

        for (iteration = 0; iteration < max_iter; iteration++) {
            double next_z_real = z_real * z_real - z_imaginary * z_imaginary + pos_x;
            z_imaginary = pos_y + 2.0 * z_real * z_imaginary;
            z_real = next_z_real;

            if (z_real * z_real + z_imaginary * z_imaginary > 4.0) {
                break;
            }
        }
        out_row[px] = iteration;
    }
}

void reorder_cyclic(const int32_t* gathered, int32_t* ordered, int size, int nprocs){
    int n = size/nprocs;

    for (int r = 0; r< nprocs; r++){
        for (int k = 0; k < n; k++){
            int global_line = r + k * nprocs;
            int received_line = r * n + k;
            memcpy(ordered + (size_t)global_line*size,
                    gathered + (size_t)received_line * size,
                    (size_t)size * sizeof(int32_t));
        }
    }
}