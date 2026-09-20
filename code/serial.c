
/*
This file has the serial functions 
*/

#include "settings.c"

//Functions declarations
void write_escape_array_file(int16_t* pointer,int tam ,char* output_name);

void calc_escape_time(int16_t* array, double x_max, double x_min, double y_max, double y_min, int max_iter);


//Functions implementations

void write_escape_array_file(int16_t* pointer,int tam ,char* output_name){
    //defining the directory with the bin
    char output_directory[256];
    snprintf(output_directory, sizeof(output_directory), "%s/%s", OUTPUT_ESCAPE_BIN, output_name);

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

void calc_escape_time(int16_t* array, double x_max, double x_min, double y_max, double y_min, int max_iter){
    double z_new = 0, z_distance; //complex position of the "pixel"
    int i; //actual pixel position (in image)
       
    double x_step_size = (x_max - x_min)/MAX_COLUMNS;
    double y_step_size = (y_max - y_min)/ MAX_ROWS;

    for (int py=0;py < MAX_ROWS ; py++){
        double pos_y = y_min + (py * y_step_size); // actual position in the plane like: -1.5 + (0 * 0.007)

        for (int px=0; px < MAX_COLUMNS; px++){
            double pos_x = x_min + (px*x_step_size); // the same as above, actual position in the plane

            double z_real=0, z_imaginary=0;
            //calculate if it is on the set
            for(i=0;i<max_iter;i++){
                //zn = zr + zi;
                double temp_z_real = z_real*z_real - (z_imaginary * z_imaginary) + pos_x;
                z_imaginary = pos_y + 2.0*z_real*z_imaginary; 
                z_real = temp_z_real;

                //to calc if it escapes we need |zn|, as it is a complex number
                z_distance = z_real*z_real + z_imaginary*z_imaginary;
                if (z_distance > 4.0) break;
            }
            array[MAX_COLUMNS*py + px] = i; // the matrix is declared as row major

        }

    }

}
