#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FILE_SIZE 4 * 1024 * 1024 // 10 MB in bytes
#define FILENAME "/mnt/ouichefs/output.txt"

void write_file();
void read_file();

int main() {
    write_file();
    read_file();
    return 0;
}

void write_file() {
    FILE *file;
    char *buffer;
    size_t buffer_size = FILE_SIZE;
    clock_t start_time, end_time;
    double write_time;

    // Allocate memory for the buffer
    buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Fill the buffer with 'A's
    for (size_t i = 0; i < buffer_size; i++) {
        buffer[i] = 'A';
    }

    // Open the file for writing
    file = fopen(FILENAME, "wb");
    if (file == NULL) {
        perror("Failed to open file");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    // Measure the write time
    start_time = clock();
    fwrite(buffer, 1, buffer_size, file);
    end_time = clock();

    // Calculate the write time in seconds
    write_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    // Output the write time
    printf("Time taken to write 10MB: %f seconds\n", write_time);

    // Clean up
    fclose(file);
    free(buffer);
}

void read_file() {
    FILE *file;
    char *buffer;
    size_t buffer_size = FILE_SIZE;
    clock_t start_time, end_time;
    double read_time;

    // Allocate memory for the buffer
    buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Open the file for reading
    file = fopen(FILENAME, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    // Measure the read time
    start_time = clock();
    fread(buffer, 1, buffer_size, file);
    end_time = clock();

    // Calculate the read time in seconds
    read_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    // Output the read time
    printf("Time taken to read 10MB: %f seconds\n", read_time);

    // Clean up
    fclose(file);
    free(buffer);
}
