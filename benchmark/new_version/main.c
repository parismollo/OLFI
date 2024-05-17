#include "benchmark.h"
#include <stdlib.h>

int main(int argc, char** agv) {
    setup("test_files/", 3);
    int ret = system("cp -r test_files/ /mnt/ouichefs/");
    if (ret != 0) {
        perror("cp command failed");
        return EXIT_FAILURE;
    }

    printf("Running benchmark on original test_files directory...\n");
    run_benchmark("test_files/", 3);

    printf("Running benchmark on /mnt/ouichefs/test_files directory...\n");
    run_benchmark("/mnt/ouichefs/test_files/", 3);
    return 0;
}