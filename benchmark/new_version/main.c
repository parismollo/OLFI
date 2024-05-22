#include "benchmark.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_files>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_files = atoi(argv[1]);
    if (num_files <= 0) {
        fprintf(stderr, "Invalid number of files: %d\n", num_files);
        return EXIT_FAILURE;
    }

    setup("test_files/", num_files);

    int ret = system("cp -r test_files/ /mnt/ouichefs/");
    if (ret != 0) {
        perror("cp command failed");
        return EXIT_FAILURE;
    }

    printf(" ------------------------------------------------- \n");
    printf("| Benchmark test for ext4: performance read/write |\n");
    printf(" ------------------------------------------------- \n");
    run_benchmark("test_files/", num_files);
    printf("\n");

    printf(" -----------------------------------------------------\n");
    printf("| Benchmark test for ouichefs: performance read/write |\n");
    printf(" -----------------------------------------------------\n");
    run_benchmark("/mnt/ouichefs/test_files/", num_files);
    printf("\n");

    printf(" --------------------------- \n");
    printf("| Check read/write for ext4 |\n");
    printf(" --------------------------- \n");
    run_write_read_check("test_files/", "test_files/check_result", num_files);
    printf("\n");

    printf(" ------------------------------- \n");
    printf("| Check read/write for ouichefs |\n");
    printf(" ------------------------------- \n");
    run_write_read_check("/mnt/ouichefs/test_files/", "/mnt/ouichefs/test_files/check_result", num_files);
    printf("\n");
    
    return 0;
}
