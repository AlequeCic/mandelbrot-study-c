#include "serial.c"

const int max_errors = MAX_ROWS * MAX_COLUMNS * 0.0001;

//Function declarations
void check_output(int16_t* my_output, int16_t* teacher_output);

void read_output(int16_t* array, int tam, char* file_name);

//main
int main(void){
    int16_t* my_output, teacher_output;
    
    array_alloc(&my_output);
    array_alloc(&teacher_output);

    read_output(my_output, MAX_COLUMNS*MAX_ROWS,MY_ESCAPE_FILE_NAME);
    read_output(teacher_output, MAX_COLUMNS*MAX_ROWS,TEACHER_ESCAPE_FILE_NAME);

    check_output(my_output,teacher_output);

    printf("Didn't find an error");

    free(my_output);
    free(teacher_output);

    return 0;
}

//Functions implementation
void check_output(int16_t* my_output, int16_t* teacher_output){

    int error_count = 0;

    for (int i =0; i < MAX_ROWS;i++){
        for (int j=0; j<MAX_COLUMNS; j++){

            if (my_output[MAX_ROWS*i + j] != teacher_output[MAX_ROWS*i + j]){
                if (abs(my_output[MAX_ROWS*i + j] - teacher_output[MAX_ROWS*i + j]) > 1){
                    printf("");
                    exit(1);
                }
                error_count++;
                
                if (error_count >= max_errors){
                    printf("");
                    exit(1);
                }
            }

        }
    }


}

void read_output(int16_t* array, int tam, char* file_name){
    char output_directory[256];
    snprintf(output_directory,sizeof(output_directory),"%s/%s", OUTPUT_BIN, file_name);

    FILE* fp = fopen(output_directory,"rb");
    if (fp){
        fread(array, sizeof(int16_t), tam, fp);
    }
    else{
        printf("Din't find file named %s", output_directory);
    }
}