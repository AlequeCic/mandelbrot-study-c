
/*
This file has the serial functions 
*/

#include "settings.c"

//Functions declarations
void write_escape_array_file(int16_t* pointer,int tam ,char* output_name);

void calc_escape_time(int16_t* array);
void calc_escape_time_3_multi(int16_t* array);


//Functions implementations

void write_escape_array_file(int16_t* pointer,int tam ,char* output_name){
    //defining the directory with the bin
    char output_directory[256];
    snprintf(output_directory, sizeof(output_directory), "%s/%s", OUTPUT_BIN, output_name);

    //opening the file in write binary mode
    FILE* fp = fopen(output_directory, "wb"); 

    if (fp){
        //writing in int size as the teacher asked to
        fwrite(pointer, sizeof(int16_t), tam, fp);
        fclose(fp);
    }

    else{
        printf("file can't be opened");
    }

}

void calc_escape_time(int16_t* array){
    double pos_x, pos_y, z_real=0, z_imaginary=0, z_new = 0, z_distance; //complex position of the "pixel"
    int px,py, i; //actual pixel position (in image)
       
    double x_step_size = (X_MAX - X_MIN)/MAX_COLUMNS;
    double y_step_size = (Y_MAX - Y_MIN)/ MAX_ROWS;

    for (py=0;py < MAX_ROWS ; py++){
        pos_y = Y_MIN + (py * y_step_size); // actual position in the plane like: -1.5 + (0 * 0.007)

        for (px=0; px < MAX_COLUMNS; px++){
            pos_x = X_MIN + (px*x_step_size); // the same as above, actual position in the plane

            z_real = pos_x; z_imaginary = pos_y, z_distance=0;

            //calculate if it is on the set
            for(i=0;i<MAX_ITER;i++){
                
                
                //zn = zr + zi;
                double temp_z_real = z_real*z_real - (z_imaginary * z_imaginary) + pos_x;
                z_imaginary = pos_y + 2*z_real*z_imaginary; 
                z_real = temp_z_real;

                //to calc if it escapes we need |zn|, as it is a complex number
                z_distance = z_real*z_real + z_imaginary*z_imaginary;
                if (z_distance > 4.0) break;
            }
            array[MAX_ROWS*py + px] = i; // the matrix is declared as row major

        }

    }

}

void calc_escape_time_3_multi(int16_t* array){
    double pos_x, pos_y, z_real=0, z_imaginary=0; //complex position of the "pixel"
    double z_2_real=0, z_2_imaginary = 0, z_ri = 0;
    int px,py, i; //actual pixel position (in image)
       
    double x_step_size = (X_MAX - X_MIN)/MAX_COLUMNS;
    double y_step_size = (Y_MAX - Y_MIN)/ MAX_ROWS;

    #pragma omp parallel for schedule(dynamic, 1)
    for (py=0;py < MAX_ROWS ; py++){
        pos_y = Y_MIN + (py * y_step_size); // actual position in the plane like: -1.5 + (0 * 0.007)

        for (px=0; px < MAX_COLUMNS; px++){
            pos_x = X_MIN + (px*x_step_size); // the same as above, actual position in the plane

            z_real = pos_x; z_imaginary = pos_y;
            //calculate if it is on the set
            for(i=0;i<MAX_ITER;i++){
                
                //zn = zr + zi;
                z_2_real = z_real*z_real; // z_real^2
                z_2_imaginary = z_imaginary * z_imaginary; // 
                
                
                z_ri = z_real * z_imaginary;
                z_real = z_2_real - z_2_imaginary + pos_x;
                z_imaginary = z_ri + z_ri + pos_y; 
                if (z_2_real + z_2_imaginary > 4.0) break;
                                
            }
            array[MAX_COLUMNS*py + px] = i; // the matrix is declared as row major

        }

    }
}
