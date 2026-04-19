// interruptLinuxLibrary.c
#include "interruptLinuxLibrary.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int initializeUIOManager(UIOManager *manager) {
    for (int i = 0; i < NUM_UIO_DEVICES; ++i) {
        static char paths[NUM_UIO_DEVICES][16];
        snprintf(paths[i], sizeof(paths[i]), "/dev/uio%d", FIRST_UIO_INDEX + i);
        manager->device_paths[i] = paths[i];

        manager->fds[i] = open(paths[i], O_RDWR);
        if (manager->fds[i] < 0) {
            perror("open");
            return -1;
        }
    }
    return 0;
}

int setupInterrupts(UIOManager *manager) {
    for (int i = 0; i < NUM_UIO_DEVICES; ++i) {
        uint32_t info = 1; // unmask
        ssize_t nb = write(manager->fds[i], &info, sizeof(info));
        if (nb != (ssize_t)sizeof(info)) {
            perror("write");
            return -1;
        }
    }
    return 0;
}

int waitInterrupt(int fd_index, int fd) {
    uint32_t info;
    ssize_t nb = read(fd, &info, sizeof(info));
    if (nb == (ssize_t)sizeof(info)) {
        printf("Interrupt received on UIO%d: #%u\n", FIRST_UIO_INDEX + fd_index, info);
        return 0;
    }
    perror("read");
    return -1;
}

int waitInterruptUIO4(UIOManager *manager) { return waitInterrupt(INDEX_UIO4, manager->fds[0]); }
int waitInterruptUIO5(UIOManager *manager) { return waitInterrupt(INDEX_UIO5, manager->fds[1]); }
int waitInterruptUIO6(UIOManager *manager) { return waitInterrupt(INDEX_UIO6, manager->fds[2]); }
int waitInterruptUIO7(UIOManager *manager) { return waitInterrupt(INDEX_UIO7, manager->fds[3]); }
int waitInterruptUIO8(UIOManager *manager) { return waitInterrupt(INDEX_UIO8, manager->fds[4]); }
int waitInterruptUIO9(UIOManager *manager) { return waitInterrupt(INDEX_UIO9, manager->fds[5]); }