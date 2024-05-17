#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int max_benchmark_run = 500;
const int MAX_BENCHMARK_TEST_FILE  = 6;
const char DOUBLE_PRECISION_DISPLAY[10] = "%.7f";
const char OUTPUTFILENAME_PREFIX[20] = "output_file_";
const char INPUTFILENAME_PREFIX[20] = "input_file_";
const char WRITE_RESULT_FILE[20] = "write_results.csv";
const char READ_RESULT_FILE[20] = "read_results.csv";
char WRITE_AVERAGE_RESULT_FILE[20] = "write_average.csv";
char READ_AVERAGE_RESULT_FILE[20] = "read_average.csv";
const char COLUMN_SEPARATOR = '|';

struct bench_mark_input_file {
    char inputfilename[20];
    char outputfilename[20];
    int offset;
    int file_index;
};

char * readfilefromposition(char* filepath, long offset){
        
    FILE *fp;
    long lSize;
    char *buffer;

    fp = fopen (filepath, "r" );
    if(!fp){
        perror("Error while opening input file !!!");
        exit(1);
    }

    fseek(fp , 0L , SEEK_END);
    lSize = ftell(fp);
    rewind(fp);

    fseek(fp , offset , SEEK_CUR);

    /* allocate memory for entire content */
    buffer = (char *)calloc(1, lSize +1);
    if(!buffer){
        fclose(fp);
        fputs("memory allocation fails", stderr);
        exit(1);
    }

    /* copy the file into the buffer */
    if(1 != fread(buffer , lSize - offset, 1 , fp)){
        fclose(fp);
        fputs("entire read fails",stderr);
        exit(1);
    }

    /* do your work here, buffer is a string contains the whole text */
    fclose(fp);
    return buffer;
}

char * readinputfile(char* filepath){
        
    FILE *fp;
    long lSize;
    char *buffer;

    fp = fopen (filepath, "r" );
    if(!fp){
        perror("Error while opening input file !!!");
        exit(1);
    }

    fseek(fp , 0L , SEEK_END);
    lSize = ftell(fp);
    rewind(fp);

    /* allocate memory for entire content */
    buffer = (char *)calloc(1, lSize+1);
    if(!buffer){
        fclose(fp);
        fputs("memory allocation fails", stderr);
        exit(1);
    }

    /* copy the file into the buffer */
    if(1 != fread(buffer , lSize, 1 , fp)){
        fclose(fp);
        fputs("entire read fails",stderr);
        exit(1);
    }

    /* do your work here, buffer is a string contains the whole text */
    fclose(fp);
    return buffer;
}

struct bench_mark_input_file* write_benchmark_file(int file_index, int offset, char* partition_prefix, double* write_results){
    
    struct bench_mark_input_file* bmif = (struct bench_mark_input_file*)malloc(sizeof(struct bench_mark_input_file)); 
    bmif->offset = offset;
    bmif->file_index = file_index;
    strcpy(bmif->inputfilename, INPUTFILENAME_PREFIX);
    strcat(bmif->inputfilename, "%d");
    sprintf(bmif->inputfilename, bmif->inputfilename, file_index);
    
    strcpy(bmif->outputfilename, partition_prefix);
    strcat(bmif->outputfilename, OUTPUTFILENAME_PREFIX);
    strcat(bmif->outputfilename, "%d");
    sprintf(bmif->outputfilename, bmif->outputfilename, file_index);

    printf("Built output is %s.\n", bmif->outputfilename);

    FILE *file;
    char* data = readinputfile(bmif->inputfilename);
    
    //start recording time spent
    clock_t begin = clock();
    file = fopen(bmif->outputfilename, "w");
    if (file == NULL) {
        printf("Error creating file.\n");
        exit(1);
    }
    fseek(file, sizeof(data)/sizeof(char) - 1, SEEK_SET); // Move file pointer to end
    fputc('\0', file); // Write null character to extend file to given offset
    fclose(file);

    // Open file in append mode to add data at different positions
    file = fopen(bmif->outputfilename, "a");
    if (file == NULL) {
        printf("Error opening file.\n");
        exit(1);
    }
    // Move file pointer to specified position
    fseek(file, bmif->offset, SEEK_SET);

    int result = fputs(data, file);
    if (result == EOF) {
        printf("Error opening file.\n");
        exit(1);
    }
    fclose(file);
    //end recording time spent
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent is %f \n", time_spent);
    write_results[file_index] = time_spent;

    if (data){
        free(data);
    }

    printf("Data written successfully.\n");
    return bmif;
}

//may be it will be a good idea to read the files from differents offset, not neccessarly the one we began writting
void read_benchmark_file(struct bench_mark_input_file* bmif, double* read_results){

    //start recording time spent
    clock_t begin = clock();
    char* data = readfilefromposition(bmif->outputfilename, bmif->file_index);

    //end recording time spent
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent is %f \n", time_spent);
    read_results[bmif->file_index] = time_spent;

    printf("Data read successfully.\n");
}

void register_write_results(double* write_results){
    FILE *result_file;
    printf("Recorded results are.\n");
    for (size_t i = 0; i < MAX_BENCHMARK_TEST_FILE; i++)
    {
        printf("Index %ld ====> %f .\n", i, write_results[i]);
    }
    
    
    char buf[20];
    char row[200];
    row[0] = '\0';
    for(size_t i = 0; i < MAX_BENCHMARK_TEST_FILE; i++){
        sprintf(buf, DOUBLE_PRECISION_DISPLAY, write_results[i]);
        strcat(row, buf);
        if (i < MAX_BENCHMARK_TEST_FILE - 1){
            strcat(row, "|");
        } else{
            strcat(row, "\n");
        }
    }

    result_file = fopen(WRITE_RESULT_FILE, "a");
    if (!result_file) {
        printf("Error opening write results file.\n");
        exit(1);
    }

    int result = fputs(row, result_file);

    if (result == EOF) {
        printf("Error writing file.\n");
        fclose(result_file);
        exit(1);
    }
    fclose(result_file);
}

void register_read_results(double* read_results){
    FILE *result_file;
    printf("Recorded results are.\n");
    for (size_t i = 0; i < MAX_BENCHMARK_TEST_FILE; i++)
    {
        printf("Index %ld ====> %f .\n", i, read_results[i]);
    }
    
    char buf[20];
    char row[200];
    row[0] = '\0';
    for(size_t i = 0; i < MAX_BENCHMARK_TEST_FILE; i++){
        sprintf(buf, DOUBLE_PRECISION_DISPLAY, read_results[i]);
        strcat(row, buf);
        if (i < MAX_BENCHMARK_TEST_FILE - 1){
            strcat(row, "|");
        } else{
            strcat(row, "\n");
        }
    }

    result_file = fopen(READ_RESULT_FILE, "a");
    if (!result_file) {
        printf("Error opening read results file.\n");
        exit(1);
    }

    int result = fputs(row, result_file);

    if (result == EOF) {
        printf("Error reading file.\n");
        fclose(result_file);
        exit(1);
    }
    fclose(result_file);
}

void register_average_results(double* results, char* filepath){
    FILE *result_file;
    
    char buf[20];
    char row[100];
    row[0] = '\0';
    for(size_t i = 0; i < MAX_BENCHMARK_TEST_FILE; i++){
        double average = results[i]/max_benchmark_run;
        sprintf(buf, DOUBLE_PRECISION_DISPLAY, average);
        strcat(row, buf);
        if (i < MAX_BENCHMARK_TEST_FILE - 1){
            strcat(row, "|");
        } else{
            strcat(row, "\n");
        }
    }

    result_file = fopen(filepath, "w");
  if (!result_file) {
      printf("Error opening read results file.\n");
      exit(1);
  }

    int result = fputs(row, result_file);

    if (result == EOF) {
        printf("Error reading file.\n");
        fclose(result_file);
        exit(1);
    }
    fclose(result_file);
}

void run_benchmark(char* partition_prefix, double* write_results_average, double* read_results_average){
    double* write_results = (double*)malloc(sizeof(double) * MAX_BENCHMARK_TEST_FILE);
    struct bench_mark_input_file** bmifs = (struct bench_mark_input_file**)malloc(sizeof(struct bench_mark_input_file*) * MAX_BENCHMARK_TEST_FILE);
    int offset = 0;
    for (int i = 0; i < MAX_BENCHMARK_TEST_FILE; i++){
        // We should fine a better way to test a whole variety of offsets
        struct bench_mark_input_file* bmif = write_benchmark_file(i, (50*i) + offset, partition_prefix, write_results);
        bmifs[i] = bmif;
        write_results_average[i] = write_results_average[i] + write_results[i];
    }
    register_write_results(write_results);
    free(write_results);

    double* read_results = (double*)malloc(sizeof(double) * MAX_BENCHMARK_TEST_FILE);
    for (int i = 0; i < MAX_BENCHMARK_TEST_FILE; i++){
        read_benchmark_file(bmifs[i], read_results);
        read_results_average[bmifs[i]->file_index] = read_results_average[bmifs[i]->file_index] + read_results[bmifs[i]->file_index];
    }
    register_read_results(read_results);
    free(read_results);

    for (int i = 0; i < MAX_BENCHMARK_TEST_FILE; i++){
        free(bmifs[i]);
    }
    free(bmifs);
}

int main(int argc, char** argv) {

    if (argc < 2){
        printf("Invalid argument number. Partition path expected.\n");
        exit(1);
    }
    
    //argv[2] is optionnal, default value is 500
    if (argv[2]){
        max_benchmark_run = atoi(argv[2]);
    }
    
    double* write_results_average = (double*)malloc(sizeof(double) * MAX_BENCHMARK_TEST_FILE);
    double* read_results_average = (double*)malloc(sizeof(double) * MAX_BENCHMARK_TEST_FILE);
    for (size_t i = 0; i < max_benchmark_run; i++){
        run_benchmark(argv[1], write_results_average, read_results_average);
    }

    register_average_results(write_results_average, WRITE_AVERAGE_RESULT_FILE);
    register_average_results(read_results_average, READ_AVERAGE_RESULT_FILE);
    free(write_results_average);
    free(read_results_average);
    return 0;
}