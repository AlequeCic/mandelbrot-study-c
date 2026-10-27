#include "settings.h"

int array_alloc(int32_t** array, int rows, int columns) {
    if (array == NULL || rows <= 0 || columns <= 0) {
        return 0;
    }

    size_t pixel_count = (size_t)rows * (size_t)columns;
    if (pixel_count > SIZE_MAX / sizeof(**array)) {
        *array = NULL;
        return 0;
    }

    *array = malloc(sizeof(**array) * pixel_count);
    return *array != NULL;
}

int read_output(int32_t* array, size_t count, const char* file_name) {
    char output_directory[256];
    snprintf(output_directory, sizeof(output_directory), "%s/%s", OUTPUT_ESCAPE_BIN, file_name);

    FILE* fp = fopen(output_directory, "rb");
    if (fp) {
        size_t items_read = fread(array, sizeof(*array), count, fp);
        fclose(fp);
        if (items_read != count) {
            fprintf(stderr, "Unexpected size for %s\n", output_directory);
            return 0;
        }
        printf("Read the file %s with success\n", output_directory);
        return 1;
    } else {
        printf("Didn't find file named %s\n", output_directory);
        return 0;
    }
}

int write_output(const int32_t* array, size_t count, const char* file_name) {
    char output_directory[256];
    snprintf(output_directory, sizeof(output_directory), "%s/%s", OUTPUT_ESCAPE_BIN, file_name);
    
    FILE* fp = fopen(output_directory, "wb");
    if (!fp) {
        printf("Nao foi possivel abrir criar o arquivo %s\n", file_name);
        return 0;
    }

    size_t written = fwrite(array, sizeof(*array), count, fp);
    fclose(fp);

    if (written != count){
        fprintf(stderr, "escrita incompleta em %s", output_directory);
        return 0;
    }
    else{
        printf("arquivo %s salvo com sucesso", output_directory);
        return 1;
    }
}