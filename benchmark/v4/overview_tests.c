#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <limits.h> 



void create_file_with_random_writes(size_t file_size, char *file_name) {
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


void create_multiple_files_with_random_writes(size_t file_size, int num_files) {
    double total_time = 0.0;

    for (int i = 0; i < num_files; i++) {
        char file_name[256];
        snprintf(file_name, sizeof(file_name), "/mnt/ouichefs/randomwritefile%d", i + 1);

        clock_t start = clock();
        create_file_with_random_writes(file_size,file_name);
        clock_t end = clock();
        
        double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
        total_time += elapsed_time;
    }

    double average_time = total_time / num_files;
    printf("Average elapsed time for %d files: %f seconds\n", num_files, average_time);
}


void create_file_with_sequential_writes(size_t file_size, char *file_name) {
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

void create_multiple_files_with_sequential_writes(size_t file_size, int num_files) {
    double total_time = 0.0;

    for (int i = 0; i < num_files; i++) {
        char file_name[PATH_MAX];
        snprintf(file_name, sizeof(file_name), "/mnt/ouichefs/sequentialwritefile%d", i + 1);

        clock_t start = clock();
        create_file_with_sequential_writes(file_size, file_name);
        clock_t end = clock();

        double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
        total_time += elapsed_time;
    }

    double average_time = total_time / num_files;
    printf("Average elapsed time for %d files: %f seconds\n", num_files, average_time);
}

void read_file_and_measure_performance(char *file_name, char *read_type) {
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


void read_multiple_files_and_measure_performance(char *folder, char *prefix) {
    DIR *dir;
    struct dirent *entry;
    double total_time = 0.0;
    int file_count = 0;

    if ((dir = opendir(folder)) == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", folder, entry->d_name);
            clock_t start = clock();
            read_file_and_measure_performance(file_path, "File read");
            clock_t end = clock();
            double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
            total_time += elapsed_time;
            file_count++;
        }
    }
    closedir(dir);

    if (file_count > 0) {
        double average_time = total_time / file_count;
        printf("Average elapsed time for %d files: %f seconds\n", file_count, average_time);
    } else {
        printf("No files found with the prefix \"%s\" in the folder \"%s\".\n", prefix, folder);
    }
}

void test_fragmented_write(size_t file_size, char *file_name) {
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


void create_multiple_files_with_fragmented_writes(size_t file_size, int num_files) {
    double total_time = 0.0;

    for (int i = 0; i < num_files; i++) {
        char file_name[PATH_MAX];
        snprintf(file_name, sizeof(file_name), "/mnt/ouichefs/fragmentedwritefile%d", i + 1);

        clock_t start = clock();
        test_fragmented_write(file_size, file_name);
        clock_t end = clock();

        double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
        total_time += elapsed_time;
    }

    double average_time = total_time / num_files;
    printf("Average elapsed time for %d files: %f seconds\n", num_files, average_time);
}


void execute_program(char *program_path, char *file_path, int operation) {
    // Construct the command string
    char command[512];
    snprintf(command, sizeof(command), "%s %s %d", program_path, file_path, operation);

    // Execute the command
    int result = system(command);
    if (result == -1) {
        perror("system");
    } else {
        // printf("Program executed with result: %d\n", result);
    }
}


// Function to verify write and read operations
int verify_write_and_read(char *file_name, char *content) {
    // Open the file for writing in binary mode
    FILE *file = fopen(file_name, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return 0;
    }

    // Write content to the file
    size_t content_length = strlen(content);
    size_t written = fwrite(content, 1, content_length, file);
    if (written != content_length) {
        perror("Failed to write the full content to the file");
        fclose(file);
        return 0;
    }

    // Close the file after writing
    fclose(file);

    // Open the file for reading in binary mode
    file = fopen(file_name, "rb");
    if (!file) {
        perror("Failed to open file for reading");
        return 0;
    }

    // Allocate a buffer for reading
    char *read_buffer = (char *)malloc(content_length + 1);
    if (!read_buffer) {
        perror("Failed to allocate memory for read buffer");
        fclose(file);
        return 0;
    }

    // Read content from the file
    size_t read = fread(read_buffer, 1, content_length, file);
    if (read != content_length) {
        perror("Failed to read the full content from the file");
        free(read_buffer);
        fclose(file);
        return 0;
    }
    read_buffer[content_length] = '\0'; // Null-terminate the read content

    // Close the file after reading
    fclose(file);

    // Verify that the read content matches the written content
    int result = strcmp(content, read_buffer) == 0;
    if (!result) {
        fprintf(stderr, "Content mismatch:\nWritten: %s\nRead: %s\n", content, read_buffer);
    }else {
        printf("verify_write_and_read(): OK!\n");
    }

    // Free the read buffer
    free(read_buffer);

    return result;
}


int main() {
    size_t dataSize = 1024 * 10;
    int N = 5;
    char * program_name = "/../usr/ioctl_test";
    printf("------------START BENCHMARK V4------------\n");
    // 1. random write
    printf("(1) create_file_with_random_writes():\n");
    create_file_with_random_writes(dataSize, "/mnt/ouichefs/random_write_file");
    // 2. sequential write
    printf("(2) create_file_with_sequential_writes():\n");
    create_file_with_sequential_writes(dataSize, "/mnt/ouichefs/sequential_write_file");
    // 3. read random
    printf("(3) read_file_and_measure_performance():\n");
    read_file_and_measure_performance("/mnt/ouichefs/random_write_file", "random");
    // 4. read sequential
    printf("(4) read_file_and_measure_performance():\n");
    read_file_and_measure_performance("/mnt/ouichefs/sequential_write_file", "sequential");
    // 5. fragmented write
    printf("(5) test_fragmented_write():\n");
    test_fragmented_write(dataSize, "/mnt/ouichefs/fragmented_file");
    // 6. ioctl show blocks
    printf("(6) ioctl show blocks():\n");
    execute_program(program_name, "/mnt/ouichefs/fragmented_file", 0);
    // (7) defragmented
    printf("(7) ioclt defragmented():\n");
    execute_program(program_name, "/mnt/ouichefs/fragmented_file", 1);
    // 8. ioctl show blocks
    printf("(8) ioctl show blocks():\n");
    execute_program(program_name, "/mnt/ouichefs/fragmented_file", 0);
    // 8.b lecture defragmented
    printf("(8.b) lecture defragmented\n");
    read_file_and_measure_performance("/mnt/ouichefs/fragmented_file", "defragmented_reading");
    // 9. benchmark write sequential
    printf("(9) create_multiple_files_with_sequential_writes():\n");
    create_multiple_files_with_sequential_writes(dataSize, N);
    // 10. benchmark write random
    printf("(10) create_multiple_files_with_random_writes():\n");
    create_multiple_files_with_random_writes(dataSize, N);
    // 11. benchmark write fragmented
    printf("(11) create_multiple_files_with_fragmented_writes():\n");
    create_multiple_files_with_fragmented_writes(dataSize, N);
    // 12. benchmark read sequential
    printf("(12) read_multiple_files_and_measure_performance():\n");
    read_multiple_files_and_measure_performance("/mnt/ouichefs/", "sequentialwritefile");
    // 13. benchmark read random
    printf("(13) read_multiple_files_and_measure_performance():\n");
    read_multiple_files_and_measure_performance("/mnt/ouichefs/", "randomwritefile");
    // 14. bechmark read fragmented
    printf("(14) read_multiple_files_and_measure_performance():\n");
    read_multiple_files_and_measure_performance("/mnt/ouichefs/", "fragmentedwritefile");
    
    printf("(15) verify_write_and_read():\n");
    verify_write_and_read("/mnt/ouichefs/check_wr_function", "????   -------   XXXXXXX       éé&é&  5455454  MMMM aa&&&!!!!!!");
    printf("------------END BENCHMARK V4------------\n");
    return 0;
}