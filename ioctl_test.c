#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>

#define OUICHEFS_IOC_MAGIC 'o'
#define OUICHEFS_IOC_GET_INFO _IOR(OUICHEFS_IOC_MAGIC, 1, struct ouichefs_ioctl_info)

#define OUICHEFS_BLOCK_SIZE (1 << 12)  // 4 KiB

struct ouichefs_block_info {
    uint32_t block_number;
    uint32_t effective_size;
};

struct ouichefs_ioctl_info {
    uint32_t used_blocks;
    uint32_t partially_filled_blocks;
    uint32_t internal_fragmentation;
    struct ouichefs_block_info blocks[OUICHEFS_BLOCK_SIZE >> 2];
};

void print_ioctl_info(struct ouichefs_ioctl_info *info) {
    printf("Used blocks: %u\n", info->used_blocks);
    printf("Partially filled blocks: %u\n", info->partially_filled_blocks);
    printf("Internal fragmentation: %u bytes\n", info->internal_fragmentation);
    for (uint32_t i = 0; i < info->used_blocks; ++i) {
        printf("Block %u: %u bytes\n", info->blocks[i].block_number, info->blocks[i].effective_size);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *file_path = argv[1];
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    struct ouichefs_ioctl_info info;
    if (ioctl(fd, OUICHEFS_IOC_GET_INFO, &info) < 0) {
        perror("ioctl");
        close(fd);
        return EXIT_FAILURE;
    }

    print_ioctl_info(&info);
    close(fd);
    return EXIT_SUCCESS;
}
