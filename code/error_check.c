#include "settings.c"

const int max_errors = MAX_ROWS * MAX_COLUMNS * 0.0001;

//Function declarations
void check_output(int16_t* my_output, int16_t* teacher_output);

//main
int main(void){
    int16_t *my_output, *teacher_output;
    
    array_alloc(&my_output);
    array_alloc(&teacher_output);

    read_output(my_output, MAX_COLUMNS*MAX_ROWS,SERIAL_ESCAPE_FILE_NAME);
    //read_output(teacher_output, MAX_COLUMNS*MAX_ROWS,TEACHER_ESCAPE_FILE_NAME);

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

            if (my_output[MAX_COLUMNS*i + j] != teacher_output[MAX_COLUMNS*i + j]){
                if (abs(my_output[MAX_COLUMNS*i + j] - teacher_output[MAX_COLUMNS*i + j]) > 1){
                    printf("The pixel[%d][%d] has more than 1 of difference", i, j);
                    exit(1);
                }
                error_count++;
                
                if (error_count >= max_errors){
                    printf("Exceeded the max number of error (0.01\%): %d\n", error_count);
                    exit(1);
                }
            }

        }
    }
}