#include "settings.c"

void write_image_mono(uint8_t* array, int16_t* escape_time_array, int max_iter);

void write_image(uint8_t* array, int16_t* escape_time_array, char* file_name, int max_iter);

void image_alloc(uint8_t** array);

int main(void){
    int16_t* escape_array;
    uint8_t* image_array;
    //alloc escape time file
    array_alloc(&escape_array);
    image_alloc(&image_array);
    
    //reading output
    read_output(escape_array, MAX_ROWS*MAX_COLUMNS,SERIAL_ESCAPE_FILE_NAME);
    
    //writing file
    write_image(image_array,escape_array, IMAGE_FILE_NAME, MAX_ITER);

    //reading horse file
    read_output(escape_array, MAX_ROWS*MAX_COLUMNS, HORSE_SERIAL_ESCAPE_FILE_NAME);

    //writing horse 
    write_image(image_array, escape_array, HORSE_IMAGE_FILE_NAME, HORSE_MAX_ITER);

    return 0;
}

void image_alloc(uint8_t** array){
    *array = (uint8_t*)malloc(sizeof(uint8_t) * MAX_ROWS * MAX_COLUMNS * 3);
}

void write_image_mono(uint8_t* array, int16_t* escape_time_array, int max_iter){
    char output_directory[256];
    snprintf(output_directory,sizeof(output_directory),"%s/%s", OUTPUT_IMAGES_BIN, IMAGE_MONO_FILE_NAME);

    //discretizing each pixel
    for (int i=MAX_ROWS-1;i>=0;i--){
        for (int j=0;j<MAX_COLUMNS;j++){
            uint8_t color = abs((escape_time_array[i*MAX_COLUMNS + j] * 255)/max_iter);
            int column = j*3;
            array[i*MAX_COLUMNS*3 + column] = color; 
            array[i*MAX_COLUMNS*3 + column+1] = color; 
            array[i*MAX_COLUMNS*3 + column+2] = color;
        }
    }

    FILE* fp = fopen(output_directory, "wb");

    if (fp){
        fprintf(fp, "P6\n%d %d\n255\n", MAX_COLUMNS, MAX_ROWS);
        fwrite(array,sizeof(uint8_t),MAX_ROWS*MAX_COLUMNS*3,fp);
        fclose(fp);
    }
    else{
        printf("File can't be opened\n");
    }
}

void write_image(uint8_t* array, int16_t* escape_time_array, char* file_name, int max_iter){
    char output_directory[256];
    snprintf(output_directory,sizeof(output_directory),"%s/%s", OUTPUT_IMAGES_BIN, file_name);
    
    for(int i=MAX_ROWS-1; i>=0; i--){

        for (int j=0; j<MAX_COLUMNS; j++){
            int escape = escape_time_array[i*MAX_COLUMNS + j];

            double red=0,green=0,blue=0;
            if (escape != max_iter){
                //following Inigo Quilez cos formula
                double t = (double)escape / 20.0; //denominator number will define band color width
                double pi_2= 6.283185;
                //pallete like vik/roma
                double a = 0.5, b = 0.5, c = 1.0;
                double d[3] = {0.0,0.1,0.2};

                red = (a + b*cos(pi_2*(c*t+ d[0])))*255;
                green = (a + b*cos(pi_2*(c*t + d[1])))*255;
                blue = (a + b*cos(pi_2*(c*t + d[2])))*255;
            }
            int column = j*3;
            array[i*MAX_COLUMNS*3 + column] = (uint8_t)red;
            array[i*MAX_COLUMNS*3 + column+1] = (uint8_t)green;
            array[i*MAX_COLUMNS*3 + column+2] = (uint8_t)blue;

        }
    }

    FILE* fp = fopen(output_directory, "wb");

    if (fp){
        fprintf(fp, "P6\n%d %d\n255\n", MAX_COLUMNS, MAX_ROWS);
        fwrite(array,sizeof(uint8_t),MAX_ROWS*MAX_COLUMNS*3,fp);
        fclose(fp);
    }
    else{
        printf("File can't be opened\n");
    }
}



