/*
 * test_simple_axi_reg.c - Test del IP simple_axi_reg (P_02)
 *
 * Espera:
 *   0x00 reg0 RW
 *   0x04 reg1 RW
 *   0x08 reg2 RW
 *   0x0C reg3 RO = 0xBADC0DE5 (ID register)
 *
 * El test ID es el mas valioso: prueba que el PL esta programado con
 * NUESTRO bitstream (no basura ni PL anterior).
 *
 * Compilar: gcc -O2 -o test_simple_axi_reg test_simple_axi_reg.c
 * Ejecutar: sudo ./test_simple_axi_reg
 */

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define BASE      0xA0000000UL  /* direccion asignada por Vivado */
#define SIZE      0x1000UL
#define ID_EXPECT 0xBADC0DE5

static volatile uint32_t *m;

static uint32_t rd(uint32_t o) { __sync_synchronize(); return m[o/4]; }
static void wr(uint32_t o, uint32_t v) { m[o/4] = v; __sync_synchronize(); }

int main(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("/dev/mem"); return 1; }

    void *p = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BASE);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    m = p;

    /* Test 1: ID register */
    uint32_t id = rd(0x0C);
    int pass = 0, fail = 0;
    printf("reg3 (ID) = 0x%08X (esperado 0x%08X) %s\n",
           id, ID_EXPECT, id == ID_EXPECT ? "[OK]" : "[FAIL]");
    if (id == ID_EXPECT) pass++; else fail++;

    /* Test 2: reg0 RW */
    uint32_t vals[] = {0x11111111, 0xAAAAAAAA, 0x00000000, 0xFFFFFFFF};
    for (int r = 0; r < 3; r++) {
        for (size_t i = 0; i < sizeof(vals)/sizeof(vals[0]); i++) {
            wr(r*4, vals[i]);
            uint32_t got = rd(r*4);
            if (got == vals[i]) pass++;
            else {
                printf("reg%d write 0x%08X read 0x%08X [FAIL]\n", r, vals[i], got);
                fail++;
            }
        }
    }

    /* Test 3: reg3 RO - escribir NO debe cambiarlo */
    wr(0x0C, 0xDEADBEEF);
    uint32_t after = rd(0x0C);
    if (after == ID_EXPECT) {
        printf("reg3 RO intacto tras escritura %s\n", "[OK]");
        pass++;
    } else {
        printf("reg3 cambio a 0x%08X [FAIL] (deberia ser RO)\n", after);
        fail++;
    }

    printf("\n%d OK, %d FAIL\n", pass, fail);
    munmap(p, SIZE);
    close(fd);
    return fail == 0 ? 0 : 1;
}
