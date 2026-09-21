#include "settings.h"

//implementation

void array_alloc(int16_t** array){
    *array = (int16_t*)malloc(sizeof(int16_t) * MAX_ROWS * MAX_COLUMNS);
}

void read_output(int16_t* array, int tam, char* file_name){
    char output_directory[256];
    snprintf(output_directory,sizeof(output_directory),"%s/%s", OUTPUT_ESCAPE_BIN, file_name);

    FILE* fp = fopen(output_directory,"rb");
    if (fp){
        fread(array, sizeof(int16_t), tam, fp);
        fclose(fp);
        printf("Read the file %s with success\n", output_directory);
    }
    else{
        printf("Din't find file named %s\n", output_directory);
        exit(1);
    }
}
