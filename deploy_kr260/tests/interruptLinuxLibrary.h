#ifndef INTERRUPT_LINUX_LIBRARY_H
#define INTERRUPT_LINUX_LIBRARY_H

#include <stdint.h>
#include <sys/types.h>

#define NUM_UIO_DEVICES 6
#define FIRST_UIO_INDEX 4

// Macros para acceder a índices más visualmente
#define INDEX_UIO4 0
#define INDEX_UIO5 1
#define INDEX_UIO6 2
#define INDEX_UIO7 3
#define INDEX_UIO8 4
#define INDEX_UIO9 5

// Macro para obtener nombre del dispositivo desde índice
#define UIO_DEVICE_NAME(i) (FIRST_UIO_INDEX + (i))

// Estructura para manejar múltiples UIO
typedef struct {
    int fds[NUM_UIO_DEVICES];
    const char *device_paths[NUM_UIO_DEVICES];
} UIOManager;

// Funciones públicas
int initializeUIOManager(UIOManager *manager);
int setupInterrupts(UIOManager *manager);
int waitInterrupt(int fd_index, int fd);
int waitInterruptUIO4(UIOManager *manager);
int waitInterruptUIO5(UIOManager *manager);
int waitInterruptUIO6(UIOManager *manager);
int waitInterruptUIO7(UIOManager *manager);
int waitInterruptUIO8(UIOManager *manager);
int waitInterruptUIO9(UIOManager *manager);

#endif // INTERRUPT_LINUX_LIBRARY_H
