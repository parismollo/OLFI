#include "benchmark.h"

int main() {
    // setup("test_files/", 3);
    // run_benchmark("test_files/", 3);
    BenchmarkResult * read_benchmark; 
    read_benchmark = (BenchmarkResult *) malloc(sizeof(BenchmarkResult));
    read_performance("test_files/example", read_benchmark);
    free(read_benchmark);
}