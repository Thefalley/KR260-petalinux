/*
 * test_dma_regs.c - Verifica que AXI DMA está programado y responde
 *
 * Solo lee/escribe registros de control del DMA — no hace transferencia real.
 * Útil como "hello DMA" para confirmar que el bitstream incluye el DMA y
 * el AXI-Lite del PS puede alcanzarlo.
 *
 * Registros AXI DMA (según PG021):
 *   0x00 MM2S_DMACR   - control MM2S
 *   0x04 MM2S_DMASR   - status MM2S
 *   0x30 S2MM_DMACR   - control S2MM
 *   0x34 S2MM_DMASR   - status S2MM
 *
 * Compilar:  gcc -O2 -Wall -o test_dma_regs test_dma_regs.c
 * Ejecutar:  sudo ./test_dma_regs
 */

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DMA_BASE  0xA0000000UL
#define DMA_SIZE  0x10000UL

#define REG_MM2S_DMACR  0x00
#define REG_MM2S_DMASR  0x04
#define REG_S2MM_DMACR  0x30
#define REG_S2MM_DMASR  0x34

#define MM2S_DMACR_RS     (1u << 0)   /* Run/Stop */
#define MM2S_DMACR_RESET  (1u << 2)   /* Reset */
#define MM2S_DMASR_HALTED (1u << 0)   /* DMA halted */
#define MM2S_DMASR_IDLE   (1u << 1)   /* DMA idle */

static volatile uint32_t *mem;

static void w(uint32_t o, uint32_t v) { mem[o / 4] = v; __sync_synchronize(); }
static uint32_t r(uint32_t o)        { __sync_synchronize(); return mem[o / 4]; }

static void dump_reg(const char *name, uint32_t off) {
    printf("  %-14s (0x%02X) = 0x%08X\n", name, off, r(off));
}

int main(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    void *map = mmap(NULL, DMA_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, DMA_BASE);
    if (map == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    mem = (volatile uint32_t *)map;

    printf("AXI DMA en 0x%08lX\n\n", DMA_BASE);

    printf("=== Estado inicial ===\n");
    dump_reg("MM2S_DMACR", REG_MM2S_DMACR);
    dump_reg("MM2S_DMASR", REG_MM2S_DMASR);
    dump_reg("S2MM_DMACR", REG_S2MM_DMACR);
    dump_reg("S2MM_DMASR", REG_S2MM_DMASR);

    printf("\n=== Reset MM2S ===\n");
    w(REG_MM2S_DMACR, MM2S_DMACR_RESET);
    /* Esperar hasta que Reset se auto-limpie (PG021 lo hace) */
    int timeout = 1000;
    while ((r(REG_MM2S_DMACR) & MM2S_DMACR_RESET) && timeout-- > 0)
        usleep(100);
    if (timeout <= 0) {
        printf("  [FAIL] Reset no se limpio en 100ms\n");
    } else {
        printf("  [OK] Reset completado\n");
    }

    printf("\n=== Reset S2MM ===\n");
    w(REG_S2MM_DMACR, MM2S_DMACR_RESET);
    timeout = 1000;
    while ((r(REG_S2MM_DMACR) & MM2S_DMACR_RESET) && timeout-- > 0)
        usleep(100);
    if (timeout <= 0) {
        printf("  [FAIL] Reset S2MM no se limpio\n");
    } else {
        printf("  [OK] Reset S2MM completado\n");
    }

    printf("\n=== Estado tras reset (deben estar halted=1, idle=0) ===\n");
    dump_reg("MM2S_DMACR", REG_MM2S_DMACR);
    dump_reg("MM2S_DMASR", REG_MM2S_DMASR);
    dump_reg("S2MM_DMACR", REG_S2MM_DMACR);
    dump_reg("S2MM_DMASR", REG_S2MM_DMASR);

    uint32_t mm2s_sr = r(REG_MM2S_DMASR);
    uint32_t s2mm_sr = r(REG_S2MM_DMASR);

    printf("\n=== Validacion ===\n");
    int pass = 0, fail = 0;

    if (mm2s_sr & MM2S_DMASR_HALTED) { printf("  [OK] MM2S HALTED=1\n"); pass++; }
    else                              { printf("  [FAIL] MM2S HALTED=0\n"); fail++; }

    if (s2mm_sr & MM2S_DMASR_HALTED) { printf("  [OK] S2MM HALTED=1\n"); pass++; }
    else                              { printf("  [FAIL] S2MM HALTED=0\n"); fail++; }

    printf("\n%d OK, %d FAIL\n", pass, fail);

    /* Limpieza: dejar en halted (no lanzar transferencia) */
    munmap(map, DMA_SIZE);
    close(fd);
    return fail == 0 ? 0 : 1;
}
