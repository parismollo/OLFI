#include <stdio.h>
#include <errno.h>

int main() {
    FILE *file;
    char buffer[12];
    size_t bytesRead;

    file = fopen("/mnt/ouichefs/test", "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    bytesRead = fread(buffer, 1, sizeof(buffer), file);

    printf("Number of bytes read: %zu\n", bytesRead);
    printf("buffer: %s\n", buffer);
    fclose(file);
    return 0;
}
