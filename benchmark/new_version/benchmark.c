#include "benchmark.h"


char * generate_random_data() {
    size_t chunk_size = DATA_CHUNK_SIZE;
    char * data;
    data = (char *) malloc(chunk_size);
    if(data == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    for(size_t i = 0; i < chunk_size; i++) {
        data[i] = 'A' + (i % 26); 
    }
    return data;
}

void create_random_file(const char * path, size_t file_size) {
    FILE *file;
    char * data;
    size_t written_size = 0;
    size_t random_pos;
    size_t chunk_size = DATA_CHUNK_SIZE;
    data = generate_random_data();
    file = fopen(path, "wb");
    if(file == NULL) {
        perror("fopen failed");
        free(data);
        exit(EXIT_FAILURE);
    }

    while(written_size < file_size) {
        random_pos = rand() % file_size;
        fseek(file, random_pos, SEEK_SET);
        fwrite(data, 1, chunk_size, file);
        written_size += chunk_size;
    }

    fclose(file);
    free(data);
}


void read_performance(const char * path, BenchmarkResult * result) {
    FILE * file;
    char * data;
    size_t chunk_size = DATA_CHUNK_SIZE;
    size_t file_size;
    clock_t start_time, end_time;

    size_t num_chunks = 3;
    size_t offsets[num_chunks];

    data = (char *) malloc(chunk_size);
    if(data == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    file = fopen(path, "rb");
    if(file == NULL) {
        perror("fopen failed");
        free(data);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    compute_offsets(file_size, chunk_size, num_chunks, offsets);

    start_time = clock();

    for(size_t i = 0; i < num_chunks; i++) {
        fseek(file, offsets[i], SEEK_SET);
        size_t bytes_read = fread(data, 1, chunk_size, file);
        if(bytes_read != chunk_size) {
            perror("fread failed");
            break;
        }
    }

    end_time = clock();

    result->time = (double)(end_time -  start_time) / CLOCKS_PER_SEC;
    fclose(file);
    free(data);
}

void compute_offsets(size_t file_size, size_t chunk_size, size_t num_chunks, size_t offsets[]) {
    size_t step = file_size / num_chunks;
    for(size_t i = 0; i < num_chunks; i++) {
        offsets[i] = i * step;
        if(offsets[i] + chunk_size > file_size) {
            offsets[i] = file_size - chunk_size;
        }
    }
}

void write_performance(const char * path, BenchmarkResult * result) {
    FILE * file;
    char * data;
    size_t chunk_size = DATA_CHUNK_SIZE;
    size_t num_chunks = 3;
    size_t offsets[num_chunks];
    clock_t start_time, end_time;
    size_t file_size = num_chunks * chunk_size;

    data = (char *)malloc(chunk_size);
    if(data == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < chunk_size; ++i) {
        data[i] = 'A' + (i % 26);
    }

    compute_offsets(file_size, chunk_size, num_chunks, offsets);

    file = fopen(path, "wb");
    if(file == NULL) {
        perror("fopen failed");
        free(data);
        exit(EXIT_FAILURE);
    }
    
    start_time = clock();
    for(size_t i = 0; i < num_chunks; i++) {
        fseek(file, offsets[i], SEEK_SET);
        size_t bytes_written = fwrite(data, 1, chunk_size, file);
        if(bytes_written != chunk_size) {
            perror("fwrite failed");
            break;
        }
    }

    end_time = clock();
    result->time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    fclose(file);
    free(data);
}

bool check_write_read(const char * path, const char * test_data) {
    FILE * file;
    size_t data_len = strlen(test_data);
    char * read_buffer = (char *) malloc(data_len);
    bool check_result = true;
    if(read_buffer == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    
    file = fopen(path, "wb");
    if(file == NULL) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    if(fwrite(test_data, 1, data_len, file) != data_len) {
        perror("fwrite failed");
        fclose(file);
        return -1;
    }

    fclose(file);

    file = fopen(path, "rb");
    if(file == NULL) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    if(fread(read_buffer, 1, data_len, file) != data_len) {
        perror("fread failed");
        fclose(file);
        return -1;
    }

    fclose(file);

    read_buffer[data_len] = '\0';
    
    if(strcmp(test_data, read_buffer) != 0) {
        check_result = false;
    }
    return check_result;
}

void setup(const char * folder, size_t N) {
    char file_path[512];
    size_t chunk_size = DATA_CHUNK_SIZE;

    struct stat st = {0};
    if(stat(folder, &st) == -1) {
        if(mkdir(folder, 0700) != 0) {
            perror("mkdir failed");
            exit(EXIT_FAILURE);
        }
    }

    for(size_t i = 0; i < N; i++) {
        snprintf(file_path, sizeof(file_path), "%s/file%zu", folder, i);
        size_t file_size = (i + 1) * chunk_size;
        create_random_file(file_path, file_size);
    }
}

void run_benchmark(const char * folder_path, size_t N) {
    BenchmarkResult * read_benchmarks;
    BenchmarkResult * write_benchmarks;

    char file_path[256];
    double total_read_time = 0.0;
    double total_write_time = 0.0;
    double total_avg_read_time = 0.0;
    double total_avg_write_time = 0.0;

    read_benchmarks = (BenchmarkResult *) malloc(sizeof(BenchmarkResult) * N);
    write_benchmarks = (BenchmarkResult *) malloc(sizeof(BenchmarkResult) * N);
    if(read_benchmarks == NULL || write_benchmarks == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    for(int z=0; z<10; z++) {
        DIR * dir = opendir(folder_path);
        if (dir == NULL) {
            perror("opendir failed");
            free(read_benchmarks);
            free(write_benchmarks);
            exit(EXIT_FAILURE);
        }

        struct dirent *entry;
        struct stat st;
        size_t i = 0;
        while((entry = readdir(dir)) != NULL && i < N) {
            snprintf(file_path, sizeof(file_path), "%s%s", folder_path, entry->d_name);
            if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
                read_performance(file_path, &read_benchmarks[i]);
                total_read_time += read_benchmarks[i].time;
                i++;
            }
        }
        closedir(dir);

        for (size_t j = 0; j < N; ++j) {
            snprintf(file_path, sizeof(file_path), "%swrite_file%zu", folder_path, j);
            write_performance(file_path, &write_benchmarks[j]);
            total_write_time += write_benchmarks[j].time;
        }
        total_avg_read_time+=total_read_time/N;
        total_avg_write_time+=total_write_time/N;

        total_read_time = 0;
        total_write_time = 0;
    }
    
    printf("    Average Read Time: %f seconds\n", total_avg_read_time/10);
    printf("    Average Write Time: %f seconds\n", total_avg_write_time/10);

    free(read_benchmarks);
    free(write_benchmarks);
}

int run_write_read_check(const char * folder_path, const char * result_path, int N) {

    FILE * file;
    char* test_data;
    size_t data_len;
    char path[256];
    DIR * dir = opendir(folder_path);
    if (dir == NULL) {
        perror("opendir failed during test data retrieval");
        exit(EXIT_FAILURE);
    }
    struct dirent *entry;
    struct stat st;
    size_t i = 0;
    bool check_result = true;
    while((entry = readdir(dir)) != NULL && i < N) {
        snprintf(path, sizeof(path), "%s%s", folder_path, entry->d_name);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {

            file = fopen(path, "rb");
            if(file == NULL) {
                perror("fopen failed");
                exit(EXIT_FAILURE);
            }

            data_len = ftell(file);
            if(fread(test_data, 1, data_len, file) != data_len) {
                perror("test data fread failed");
                fclose(file);
                return -1;
            }

            fclose(file);

            check_result &= check_write_read(result_path, test_data);
            i++;
        }
    }
    if(!check_result) {
        printf("    Check write/read failed\n");
    } else {
        printf("    Check write/read ok\n");
    }
    closedir(dir);
}