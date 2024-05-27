#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
/*
5. test defrag
6. test read degraf
*/


void create_file_with_random_writes(size_t file_size, const char *file_name) {
    // Open the file for writing in binary mode
    FILE *file = fopen(file_name, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Allocate a buffer with 1024 'A' characters
    char buffer[128];
    memset(buffer, 'A', sizeof(buffer));

    // Seed the random number generator
    srand((unsigned int)time(NULL));

    // Calculate the number of chunks
    size_t chunk_size = sizeof(buffer);
    size_t num_chunks = file_size / chunk_size;
    size_t remaining_bytes = file_size % chunk_size;

    clock_t start = clock();
    for (size_t i = 0; i < num_chunks; i++) {
        // Generate a random position within the file
        long random_position = rand() % num_chunks * chunk_size;
        fseek(file, random_position, SEEK_SET);
        fwrite(buffer, 1, chunk_size, file);
    }

    // If there are remaining bytes, write them at a random position
    if (remaining_bytes > 0) {
        long random_position = rand() % (num_chunks + 1) * chunk_size;
        fseek(file, random_position, SEEK_SET);
        fwrite(buffer, 1, remaining_bytes, file);
    }
    clock_t end = clock();
    double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("create_file_with_random_writes() - Elapsed time: %f seconds\n", elapsed_time);

    // Close the file
    fclose(file);
}


void create_file_with_sequential_writes(size_t file_size, const char *file_name) {
    // Open the file for writing in binary mode
    FILE *file = fopen(file_name, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Allocate a buffer with 128 'A' characters
    char buffer[128];
    memset(buffer, 'A', sizeof(buffer));

    // Calculate the number of chunks
    size_t chunk_size = sizeof(buffer);
    size_t num_chunks = file_size / chunk_size;
    size_t remaining_bytes = file_size % chunk_size;


    clock_t start = clock();
    // Write full chunks sequentially
    for (size_t i = 0; i < num_chunks; i++) {
        fwrite(buffer, 1, chunk_size, file);
    }

    // If there are remaining bytes, write them sequentially
    if (remaining_bytes > 0) {
        fwrite(buffer, 1, remaining_bytes, file);
    }

    clock_t end = clock();
    double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("create_file_with_sequential_writes() - Elapsed time: %f seconds\n", elapsed_time);

    // Close the file
    fclose(file);
}

void read_file_and_measure_performance(const char *file_name, const char *read_type) {
    // Open the file for reading in binary mode
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Allocate a buffer for reading
    char buffer[128];
    size_t chunk_size = sizeof(buffer);

    // Measure the time taken to read the file
    clock_t start = clock();
    while (fread(buffer, 1, chunk_size, file) == chunk_size);
    clock_t end = clock();

    // Calculate and display the elapsed time
    double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("%s - Elapsed time: %f seconds\n", read_type, elapsed_time);

    // Close the file
    fclose(file);
}



void test_fragmented_write(size_t file_size, const char *file_name) {
    // Open the file for writing in binary mode
    FILE *file = fopen(file_name, "wb+"); // Open in read/write mode
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Allocate a buffer with 128 'A' characters
    char buffer[128];
    memset(buffer, 'A', sizeof(buffer));

    // Calculate the number of chunks
    size_t chunk_size = sizeof(buffer);
    size_t num_chunks = file_size / chunk_size;
    size_t remaining_bytes = file_size % chunk_size;

    // Write full chunks sequentially
    for (size_t i = 0; i < num_chunks; i++) {
        fwrite(buffer, 1, chunk_size, file);
    }

    // If there are remaining bytes, write them sequentially
    if (remaining_bytes > 0) {
        fwrite(buffer, 1, remaining_bytes, file);
    }

    // Write additional data at random positions within the filled areas
    char overwrite_buffer[128];
    memset(overwrite_buffer, 'B', sizeof(overwrite_buffer));

    srand((unsigned int)time(NULL));
    clock_t start = clock();
    for (int i = 0; i < 3; i++) {
        // Generate a random position within the already written area
        long random_position = (rand() % num_chunks) * chunk_size;
        fseek(file, random_position, SEEK_SET);
        fwrite(overwrite_buffer, 1, chunk_size, file);
    }
    clock_t end = clock();
    double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("test_fragmented_write() - Elapsed time: %f seconds\n", elapsed_time);
    // Close the file
    fclose(file);
}


void execute_program(const char *program_path, const char *file_path, int operation, const char *output_file) {
    // Construct the command string
    char command[512];
    snprintf(command, sizeof(command), "%s %s %d > %s 2>&1", program_path, file_path, operation, output_file);

    // Execute the command
    int result = system(command);
    if (result == -1) {
        perror("system");
    } else {
        printf("Program executed with result: %d\n", result);
    }
}

int main() {
    const char *file_path = "test_file";
    size_t dataSize = 1024 * 1024; // 1MB
    // 1. random write
    create_file_with_random_writes(dataSize, file_path);
    // 2. sequential write

    // 3. read random

    // 4. read sequential

    // 5. fragmented write

    // 6. ioctl - defragmented

    return 0;
}