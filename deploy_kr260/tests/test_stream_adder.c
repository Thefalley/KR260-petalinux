/*
 * test_stream_adder.c - Test AXI-Lite del stream_adder en KR260
 *
 * Compilar en la placa:
 *   gcc -O2 -Wall -o test_stream_adder test_stream_adder.c
 *
 * Ejecutar:
 *   sudo ./test_stream_adder
 *
 * Requiere: xmutil loadapp stream_adder ya ejecutado
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define STREAM_ADDER_BASE  0xA0010000UL
#define STREAM_ADDER_SIZE  0x1000UL
#define REG_ADD_VALUE      0x00

static volatile uint32_t *mem;

static void reg_write(uint32_t off, uint32_t val) {
    mem[off / 4] = val;
    __sync_synchronize();
}

static uint32_t reg_read(uint32_t off) {
    __sync_synchronize();
    return mem[off / 4];
}

int main(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        fprintf(stderr, "  -> necesitas sudo\n");
        return 1;
    }

    void *map = mmap(NULL, STREAM_ADDER_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, STREAM_ADDER_BASE);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    mem = (volatile uint32_t *)map;

    printf("Mapeado AXI-Lite en 0x%08lX\n", STREAM_ADDER_BASE);

    static const uint32_t vals[] = {
        0x00000000, 0x00000001, 0x000000FF, 0x0000FFFF,
        0x12345678, 0xDEADBEEF, 0xFFFFFFFF
    };

    int pass = 0, fail = 0;
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        reg_write(REG_ADD_VALUE, vals[i]);
        uint32_t r = reg_read(REG_ADD_VALUE);
        if (r == vals[i]) {
            printf("  [OK]   write 0x%08X read 0x%08X\n", vals[i], r);
            pass++;
        } else {
            printf("  [FAIL] write 0x%08X read 0x%08X\n", vals[i], r);
            fail++;
        }
    }

    printf("\nRESULTADO: %d OK, %d FAIL\n", pass, fail);

    munmap(map, STREAM_ADDER_SIZE);
    close(fd);

    return fail == 0 ? 0 : 1;
}
