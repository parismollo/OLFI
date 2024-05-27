#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>

#define OUICHEFS_IOC_MAGIC 'o'
#define OUICHEFS_IOC_GET_INFO _IOR(OUICHEFS_IOC_MAGIC, 1, struct ouichefs_ioctl_info)
#define OUICHEFS_IOC_GET_DEFRAG _IOWR(OUICHEFS_IOC_MAGIC, 2, struct ouichefs_ioctl_info)

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
        printf("Block %u: ", info->blocks[i].block_number);
        if(info->blocks[i].effective_size == 0) {
            printf("4096 bytes\n");
        } else {
            printf("%u bytes\n", info->blocks[i].effective_size);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path> <operation>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *file_path = argv[1];
    int operation = atoi(argv[2]);
    if (operation != 0 && operation != 1) {
        fprintf(stderr, "Invalid operation. Use 0 for info, 1 for defragmentation.\n");
        return EXIT_FAILURE;
    }

    int fd = open(file_path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    struct ouichefs_ioctl_info info;
    if (operation == 0) {
        if (ioctl(fd, OUICHEFS_IOC_GET_INFO, &info) < 0) {
            perror("ioctl OUICHEFS_IOC_GET_INFO");
            close(fd);
            return EXIT_FAILURE;
        }
        print_ioctl_info(&info);
    } else if (operation == 1) {
        if (ioctl(fd, OUICHEFS_IOC_GET_DEFRAG, &info) < 0) {
            perror("ioctl OUICHEFS_IOC_GET_DEFRAG");
            close(fd);
            return EXIT_FAILURE;
        }
        printf("Defragmentation completed.\n");
        // print_ioctl_info(&info);
    }

    close(fd);
    return EXIT_SUCCESS;
}
