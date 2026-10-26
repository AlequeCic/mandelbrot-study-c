#ifndef SERIAL_H
#define SERIAL_H

#include "settings.h"

int write_escape_array_file(const int32_t* pointer, size_t count,
                            const char* output_name);
void calc_escape_time(int32_t* array, int rows, int columns,
                      double x_max, double x_min, double y_max,
                      double y_min, int max_iter);

#endif
