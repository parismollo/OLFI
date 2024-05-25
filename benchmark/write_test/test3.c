#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE* file = fopen("/mnt/ouichefs/test", "w");
    if (file == NULL) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    char buffer[10];
    memset(buffer, 'a', sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    fseek(file, 15, SEEK_SET);
    fwrite(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    return EXIT_SUCCESS;
}
