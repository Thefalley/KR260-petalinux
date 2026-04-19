/*
 * test_dma_full.c - Test DMA completo del stream_adder (MM2S -> PL -> S2MM)
 *
 * Verifica el flujo completo:
 *   1. Aloca buffer contiguo via u-dma-buf (/dev/udmabuf0)
 *   2. Llena buffer de entrada con valores conocidos
 *   3. Programa AXI DMA MM2S para leer buffer y enviarlo al stream_adder
 *   4. Programa AXI DMA S2MM para recibir resultado y guardarlo en otra region
 *   5. Espera completado via polling de DMASR.IDLE
 *   6. Verifica que output = input + add_value
 *
 * Requiere:
 *   - xmutil loadapp stream_adder (bitstream cargado)
 *   - modulo u-dma-buf cargado: sudo insmod /lib/modules/.../u-dma-buf.ko udmabufs=udmabuf0=2097152
 *     (2 MB - suficiente para 512K samples de 32-bit)
 *   - sudo (para /dev/mem)
 *
 * Compilar: gcc -O2 -Wall -o test_dma_full test_dma_full.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DMA_BASE          0xA0000000UL
#define DMA_SIZE          0x10000UL
#define ADDER_BASE        0xA0010000UL
#define ADDER_SIZE        0x1000UL

/* Registros AXI DMA (PG021) */
#define REG_MM2S_DMACR    0x00
#define REG_MM2S_DMASR    0x04
#define REG_MM2S_SA       0x18  /* source addr */
#define REG_MM2S_LENGTH   0x28  /* transfer length */
#define REG_S2MM_DMACR    0x30
#define REG_S2MM_DMASR    0x34
#define REG_S2MM_DA       0x48  /* destination addr */
#define REG_S2MM_LENGTH   0x58

#define DMACR_RS          (1u << 0)
#define DMACR_RESET       (1u << 2)
#define DMASR_HALTED      (1u << 0)
#define DMASR_IDLE        (1u << 1)
#define DMASR_ERR_MASK    (0x70u << 4)  /* bits 4-6 */

#define NUM_SAMPLES       256
#define XFER_BYTES        (NUM_SAMPLES * 4)
#define BUFFER_SIZE       (2 * 1024 * 1024)  /* 2 MB total */
#define MM2S_OFFSET       0
#define S2MM_OFFSET       (1 * 1024 * 1024)  /* 2nd half */

#define ADD_VALUE         0x100  /* valor a sumar */

static volatile uint32_t *dma;
static volatile uint32_t *adder;
static uint32_t *udma_virt;
static uint64_t udma_phys;

static void dma_w(uint32_t o, uint32_t v) { dma[o/4] = v; __sync_synchronize(); }
static uint32_t dma_r(uint32_t o) { __sync_synchronize(); return dma[o/4]; }
static void adder_w(uint32_t o, uint32_t v) { adder[o/4] = v; __sync_synchronize(); }

static int read_u64(const char *path, uint64_t *out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int n = fscanf(f, "%lx", (unsigned long *)out);
    fclose(f);
    return n == 1 ? 0 : -1;
}

static int wait_idle(uint32_t off, int timeout_ms) {
    while (timeout_ms-- > 0) {
        uint32_t sr = dma_r(off);
        if (sr & DMASR_ERR_MASK) {
            fprintf(stderr, "ERROR DMA SR=0x%08X\n", sr);
            return -1;
        }
        if (sr & DMASR_IDLE) return 0;
        usleep(1000);
    }
    return -1;
}

int main(void) {
    int fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd_mem < 0) { perror("/dev/mem"); return 1; }

    /* Mapear DMA y adder */
    void *p = mmap(NULL, DMA_SIZE, PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd_mem, DMA_BASE);
    if (p == MAP_FAILED) { perror("mmap DMA"); return 1; }
    dma = (volatile uint32_t *)p;

    p = mmap(NULL, ADDER_SIZE, PROT_READ | PROT_WRITE,
             MAP_SHARED, fd_mem, ADDER_BASE);
    if (p == MAP_FAILED) { perror("mmap adder"); return 1; }
    adder = (volatile uint32_t *)p;

    /* Obtener direccion fisica del udmabuf */
    if (read_u64("/sys/class/u-dma-buf/udmabuf0/phys_addr", &udma_phys) < 0) {
        fprintf(stderr, "ERROR: no se puede leer phys_addr de udmabuf0\n");
        fprintf(stderr, "  -> Cargar el modulo: sudo modprobe u-dma-buf udmabufs=udmabuf0=%u\n",
                BUFFER_SIZE);
        return 1;
    }
    printf("udmabuf0 phys_addr = 0x%lX\n", udma_phys);

    /* Mapear el udmabuf */
    int fd_udma = open("/dev/udmabuf0", O_RDWR | O_SYNC);
    if (fd_udma < 0) { perror("/dev/udmabuf0"); return 1; }
    p = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_udma, 0);
    if (p == MAP_FAILED) { perror("mmap udmabuf"); return 1; }
    udma_virt = (uint32_t *)p;
    printf("udmabuf0 mapeado: %p\n", udma_virt);

    /* Llenar buffer entrada */
    uint32_t *in_buf = (uint32_t *)((char *)udma_virt + MM2S_OFFSET);
    uint32_t *out_buf = (uint32_t *)((char *)udma_virt + S2MM_OFFSET);
    for (int i = 0; i < NUM_SAMPLES; i++) in_buf[i] = i;
    memset(out_buf, 0xFF, XFER_BYTES);   /* marcar como no escrito */

    /* Configurar stream_adder con ADD_VALUE */
    adder_w(0x00, ADD_VALUE);
    printf("stream_adder add_value = 0x%X\n", ADD_VALUE);

    /* Reset DMA */
    dma_w(REG_MM2S_DMACR, DMACR_RESET);
    while (dma_r(REG_MM2S_DMACR) & DMACR_RESET) usleep(10);
    dma_w(REG_S2MM_DMACR, DMACR_RESET);
    while (dma_r(REG_S2MM_DMACR) & DMACR_RESET) usleep(10);

    /* Arrancar canales */
    dma_w(REG_MM2S_DMACR, DMACR_RS);
    dma_w(REG_S2MM_DMACR, DMACR_RS);

    /* Programar S2MM primero (debe estar esperando antes de lanzar MM2S) */
    dma_w(REG_S2MM_DA, (uint32_t)(udma_phys + S2MM_OFFSET));
    dma_w(REG_S2MM_LENGTH, XFER_BYTES);

    /* Programar MM2S */
    dma_w(REG_MM2S_SA, (uint32_t)(udma_phys + MM2S_OFFSET));
    dma_w(REG_MM2S_LENGTH, XFER_BYTES);  /* escribir LENGTH lanza la transferencia */

    /* Esperar MM2S */
    if (wait_idle(REG_MM2S_DMASR, 5000) < 0) {
        fprintf(stderr, "TIMEOUT MM2S\n");
        return 1;
    }
    printf("MM2S completado\n");

    /* Esperar S2MM */
    if (wait_idle(REG_S2MM_DMASR, 5000) < 0) {
        fprintf(stderr, "TIMEOUT S2MM\n");
        return 1;
    }
    printf("S2MM completado\n");

    /* Verificar */
    int fail = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint32_t expected = i + ADD_VALUE;
        if (out_buf[i] != expected) {
            if (fail < 10)
                printf("[FAIL] out[%d] = 0x%08X (esperado 0x%08X)\n",
                       i, out_buf[i], expected);
            fail++;
        }
    }

    if (fail == 0) {
        printf("\n[OK] %d samples verificadas: out = in + 0x%X\n", NUM_SAMPLES, ADD_VALUE);
    } else {
        printf("\n[FAIL] %d/%d samples incorrectas\n", fail, NUM_SAMPLES);
    }

    munmap((void *)dma, DMA_SIZE);
    munmap((void *)adder, ADDER_SIZE);
    munmap(udma_virt, BUFFER_SIZE);
    close(fd_mem);
    close(fd_udma);

    return fail == 0 ? 0 : 1;
}
