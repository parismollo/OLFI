#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>

int main(){

    FILE* file = fopen("/mnt/ouichefs/test", "w");
    if(file == NULL) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }
    fwrite("abcdefg", 1, 7, file);
    // fclose(file);
    // file = fopen("/mnt/ouichefs/test", "w");
    fseek(file, 3, SEEK_SET);
    fwrite("suite", 1, 5, file);
    fclose(file);
    return EXIT_SUCCESS;
}