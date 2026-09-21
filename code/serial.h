#ifndef SERIAL_H
#define SERIAL_H

#include "settings.h"

void write_escape_array_file(int16_t* pointer, int tam, char* output_name);
void calc_escape_time(int16_t* array, double x_max, double x_min,
                      double y_max, double y_min, int max_iter);

#endif
