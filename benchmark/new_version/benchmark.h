#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define DATA_CHUNK_SIZE 1024

typedef struct {
    double time;
} BenchmarkResult;

char * generate_random_data();
void create_random_file(const char * path, size_t file_size);
void read_performance(const char * path, BenchmarkResult * result);
void write_performance(const char * path, BenchmarkResult * result);
int check_write_read(const char * path, const char * test_data);
void compute_offsets(size_t file_size, size_t chuck_size, size_t num_chunks, size_t offsets[]);
void setup(const char * folder, size_t N);
void run_benchmark(const char * folder_path, size_t N);
int run_write_read_check(const char * folder_path, const char * result_path, int num_chunks);

#endif