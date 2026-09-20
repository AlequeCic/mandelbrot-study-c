#include "settings.c"

void write_image_mono(uint8_t* array, int16_t* escape_time_array);

void write_image(uint8_t* array, int16_t* escape_time_array);

void write_image_horse(uint8_t* array, int16_t* escape_time_array);

void image_alloc(uint8_t** array);

void hsv_to_rgb(double h, double s, double v, uint8_t* r, uint8_t* g, uint8_t* b);

int main(void){
    int16_t* escape_array;
    uint8_t* image_array;
    //alloc escape time file
    array_alloc(&escape_array);
    image_alloc(&image_array);
    
    //reading output
    read_output(escape_array, MAX_ROWS*MAX_COLUMNS,SERIAL_ESCAPE_FILE_NAME);
    
    //writing mono file
    write_image(image_array,escape_array);

    return 0;
}

void image_alloc(uint8_t** array){
    *array = (uint8_t*)malloc(sizeof(uint8_t) * MAX_ROWS * MAX_COLUMNS * 3);
}

void write_image_mono(uint8_t* array, int16_t* escape_time_array){
    char output_directory[256];
    snprintf(output_directory,sizeof(output_directory),"%s/%s", OUTPUT_IMAGES_BIN, IMAGE_MONO_FILE_NAME);

    //discretizing each pixel
    for (int i=MAX_ROWS-1;i>=0;i--){
        for (int j=0;j<MAX_COLUMNS;j++){
            uint8_t color = abs((escape_time_array[i*MAX_COLUMNS + j] * 255)/MAX_ITER);
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

void write_image(uint8_t* array, int16_t* escape_time_array){
    char output_directory[256];
    snprintf(output_directory,sizeof(output_directory),"%s/%s", OUTPUT_IMAGES_BIN, IMAGE_FILE_NAME);
    
    for(int i=MAX_ROWS-1; i>=0; i--){

        for (int j=0; j<MAX_COLUMNS; j++){
            int escape = escape_time_array[i*MAX_COLUMNS + j];
            double ratio = (double)escape/MAX_ITER; //use this only if zooming
            double p = pow(ratio * 360, 1.5);

            double hsv[3] = { fmod(p,360.0), 1.0, fmin((double)escape/50.0,1.0) }; // h, s, v in each position
            uint8_t r=0, g=0, b=0;
            if (escape != MAX_ITER) hsv_to_rgb(hsv[0],hsv[1],hsv[2], &r,&g,&b);

            int column = j*3;
            array[i*MAX_COLUMNS*3 + column] = r;
            array[i*MAX_COLUMNS*3 + column+1] = g;
            array[i*MAX_COLUMNS*3 + column+2] = b;

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

void write_image_horse(uint8_t* array, int16_t* escape_time_array){
    char output_directory[256];
    snprintf(output_directory,sizeof(output_directory),"%s/%s", OUTPUT_IMAGES_BIN, IMAGE_FILE_NAME);
    
    for(int i=MAX_ROWS-1; i>=0; i--){

        for (int j=0; j<MAX_COLUMNS; j++){
            int escape = escape_time_array[i*MAX_COLUMNS + j];
            double ratio = (double)escape/MAX_ITER; //use this only if zooming
            double p = pow(ratio * 360, 1.5);

            double hsv[3] = { fmod(p,360.0), 1.0, fmin((double)escape/50.0,1.0) }; // h, s, v in each position
            uint8_t r=0, g=0, b=0;
            if (escape != MAX_ITER) hsv_to_rgb(hsv[0],hsv[1],hsv[2], &r,&g,&b);

            int column = j*3;
            array[i*MAX_COLUMNS*3 + column] = r;
            array[i*MAX_COLUMNS*3 + column+1] = g;
            array[i*MAX_COLUMNS*3 + column+2] = b;

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

void hsv_to_rgb(double h, double s, double v, uint8_t* r, uint8_t* g, uint8_t* b){
    double c = v*s;
    double x = c*(1.0 - fabs(fmod(h/60.0,2) - 1.0));

    double r_prime,g_prime,b_prime;

    double m = v - c;

    if (h >= 0 && h < 60) {r_prime = c; g_prime = x; b_prime = 0;}
    else if (h >= 60 && h < 120) {r_prime = x; g_prime = c; b_prime = 0;}
    else if (h >= 120 && h < 180) {r_prime = 0; g_prime = c; b_prime = x;}
    else if (h >= 180 && h< 240) {r_prime = 0; g_prime = x; b_prime = c;}
    else if (h >= 240 && h < 300) {r_prime = x; g_prime = 0; b_prime = c;}
    else {r_prime = c; g_prime = 0; b_prime = x;}

    *r = (uint8_t)((r_prime + m) * 255);
    *g = (uint8_t)((g_prime + m) * 255);
    *b = (uint8_t)((b_prime + m) * 255);

}