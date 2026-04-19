/*
 * test_dma_irq_full.c - Test DMA end-to-end con IRQs UIO + u-dma-buf
 *
 * Flujo:
 *  1. Abre /dev/udmabuf0 (memoria fisica contigua del CMA)
 *  2. mmap AXI DMA via /dev/uio0 (primer uio que sea DMA)
 *  3. mmap stream_adder via /dev/uio_stream_adder
 *  4. Abre /dev/uio_mm2s y /dev/uio_s2mm (solo IRQs)
 *  5. Prepara input buffer con valores 0..N-1
 *  6. Configura stream_adder add_value = ADD
 *  7. Configura DMA MM2S y S2MM (run + length)
 *  8. read() bloqueante en ambos UIO IRQs
 *  9. Verifica out[i] == i + ADD
 *
 * Requiere:
 *  - stream_adder_v3 cargado (UIO nodes creados)
 *  - u-dma-buf cargado con /dev/udmabuf0 >= 4MB
 *
 * Compilar: gcc -O2 -Wall -o test_dma_irq_full test_dma_irq_full.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>

/* DMA registers (PG021) */
#define MM2S_DMACR    0x00
#define MM2S_DMASR    0x04
#define MM2S_SA       0x18
#define MM2S_LENGTH   0x28
#define S2MM_DMACR    0x30
#define S2MM_DMASR    0x34
#define S2MM_DA       0x48
#define S2MM_LENGTH   0x58

#define DMACR_RS           0x1
#define DMACR_RESET        0x4
#define DMACR_IOC_IRQ_EN   0x1000
#define DMACR_ERR_IRQ_EN   0x4000
#define DMASR_HALTED       0x1
#define DMASR_IDLE         0x2
#define DMASR_IOC_IRQ      0x1000
#define DMASR_ERR_MASK     0x770

#define NUM_SAMPLES  256
#define XFER_BYTES   (NUM_SAMPLES * 4)
#define ADD_VALUE    0x100

/* Encuentra /dev/uioN por nombre (lee /sys/class/uio/uioN/name) */
static int find_uio_by_name(const char *name) {
    DIR *d = opendir("/sys/class/uio");
    if (!d) return -1;
    struct dirent *e;
    int found = -1;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "uio", 3) != 0) continue;
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/uio/%s/name", e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char buf[128] = {0};
        if (fgets(buf, sizeof(buf), f)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strcmp(buf, name) == 0) {
                found = atoi(e->d_name + 3);
                fclose(f);
                break;
            }
        }
        fclose(f);
    }
    closedir(d);
    return found;
}

/* Devuelve size del map0 del UIO */
static size_t get_uio_map_size(int uio_n) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/uio/uio%d/maps/map0/size", uio_n);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t sz;
    fscanf(f, "%zx", &sz);
    fclose(f);
    return sz;
}

int main(void) {
    int ret = 0;

    /* Localizar UIOs por nombre */
    int uio_dma    = find_uio_by_name("dma");        /* axi_dma_0 */
    int uio_adder  = find_uio_by_name("stream_adder");
    int uio_mm2s   = find_uio_by_name("pl_irq_mm2s");
    int uio_s2mm   = find_uio_by_name("pl_irq_s2mm");

    printf("UIO indices: dma=%d adder=%d mm2s_irq=%d s2mm_irq=%d\n",
           uio_dma, uio_adder, uio_mm2s, uio_s2mm);

    if (uio_dma < 0 || uio_adder < 0 || uio_mm2s < 0 || uio_s2mm < 0) {
        fprintf(stderr, "ERROR: no encuentro todos los UIOs. Comprobar:\n");
        fprintf(stderr, "  for i in /sys/class/uio/uio*; do echo $i $(cat $i/name); done\n");
        return 1;
    }

    /* Abrir /dev/udmabuf0 */
    int fd_udma = open("/dev/udmabuf0", O_RDWR | O_SYNC);
    if (fd_udma < 0) {
        perror("/dev/udmabuf0");
        fprintf(stderr, "  -> instalar u-dma-buf: bash install_udmabuf.sh\n");
        return 1;
    }
    uint64_t phys_addr;
    FILE *f = fopen("/sys/class/u-dma-buf/udmabuf0/phys_addr", "r");
    if (!f) { perror("phys_addr"); return 1; }
    fscanf(f, "%lx", (unsigned long*)&phys_addr);
    fclose(f);

    size_t udma_size;
    f = fopen("/sys/class/u-dma-buf/udmabuf0/size", "r");
    fscanf(f, "%zu", &udma_size);
    fclose(f);
    printf("udmabuf0 phys=0x%lX size=%zu\n", phys_addr, udma_size);

    void *buf = mmap(NULL, udma_size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd_udma, 0);
    if (buf == MAP_FAILED) { perror("mmap udmabuf"); return 1; }

    /* Abrir UIO DMA (mmap registros AXI DMA) */
    char path[64];
    snprintf(path, sizeof(path), "/dev/uio%d", uio_dma);
    int fd_dma = open(path, O_RDWR);
    if (fd_dma < 0) { perror(path); return 1; }
    size_t dma_size = get_uio_map_size(uio_dma);
    volatile uint32_t *dma = mmap(NULL, dma_size, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, fd_dma, 0);
    if (dma == MAP_FAILED) { perror("mmap dma"); return 1; }

    /* Abrir UIO stream_adder */
    snprintf(path, sizeof(path), "/dev/uio%d", uio_adder);
    int fd_adder = open(path, O_RDWR);
    if (fd_adder < 0) { perror(path); return 1; }
    size_t adder_size = get_uio_map_size(uio_adder);
    volatile uint32_t *adder = mmap(NULL, adder_size, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd_adder, 0);
    if (adder == MAP_FAILED) { perror("mmap adder"); return 1; }

    /* Abrir UIO IRQ (solo para read bloqueante, no mmap) */
    snprintf(path, sizeof(path), "/dev/uio%d", uio_mm2s);
    int fd_irq_mm2s = open(path, O_RDWR);
    snprintf(path, sizeof(path), "/dev/uio%d", uio_s2mm);
    int fd_irq_s2mm = open(path, O_RDWR);

    /* Preparar buffer entrada */
    uint32_t *in_buf  = (uint32_t *)buf;
    uint32_t *out_buf = (uint32_t *)((char *)buf + (udma_size / 2));
    uint64_t in_phys  = phys_addr;
    uint64_t out_phys = phys_addr + (udma_size / 2);

    for (int i = 0; i < NUM_SAMPLES; i++) in_buf[i] = i;
    memset(out_buf, 0xFF, XFER_BYTES);

    /* Configurar stream_adder */
    adder[0] = ADD_VALUE;
    printf("stream_adder reg0 (add_value) = 0x%X\n", adder[0]);

    /* Reset DMA */
    dma[MM2S_DMACR/4] = DMACR_RESET;
    while (dma[MM2S_DMACR/4] & DMACR_RESET) usleep(10);
    dma[S2MM_DMACR/4] = DMACR_RESET;
    while (dma[S2MM_DMACR/4] & DMACR_RESET) usleep(10);

    /* Habilitar IRQ IOC + ERR */
    dma[MM2S_DMACR/4] = DMACR_RS | DMACR_IOC_IRQ_EN | DMACR_ERR_IRQ_EN;
    dma[S2MM_DMACR/4] = DMACR_RS | DMACR_IOC_IRQ_EN | DMACR_ERR_IRQ_EN;

    /* Unmask UIO IRQs */
    uint32_t one = 1;
    write(fd_irq_mm2s, &one, 4);
    write(fd_irq_s2mm, &one, 4);

    /* Programar transferencias */
    dma[S2MM_DA/4]     = (uint32_t)out_phys;
    dma[S2MM_LENGTH/4] = XFER_BYTES;
    dma[MM2S_SA/4]     = (uint32_t)in_phys;
    dma[MM2S_LENGTH/4] = XFER_BYTES;   /* escribir length = start */

    printf("Transferencia lanzada...\n");

    /* Esperar IRQs via read bloqueante */
    uint32_t irq_count;

    ssize_t n = read(fd_irq_mm2s, &irq_count, 4);
    if (n != 4) { perror("read mm2s irq"); ret = 1; goto cleanup; }
    printf("IRQ MM2S recibida #%u (DMASR=0x%X)\n", irq_count, dma[MM2S_DMASR/4]);
    dma[MM2S_DMASR/4] = DMASR_IOC_IRQ;  /* ack */
    write(fd_irq_mm2s, &one, 4);        /* re-unmask */

    n = read(fd_irq_s2mm, &irq_count, 4);
    if (n != 4) { perror("read s2mm irq"); ret = 1; goto cleanup; }
    printf("IRQ S2MM recibida #%u (DMASR=0x%X)\n", irq_count, dma[S2MM_DMASR/4]);
    dma[S2MM_DMASR/4] = DMASR_IOC_IRQ;
    write(fd_irq_s2mm, &one, 4);

    /* Verificar */
    int fail = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint32_t expected = i + ADD_VALUE;
        if (out_buf[i] != expected) {
            if (fail < 10)
                printf("[FAIL] out[%d]=0x%08X esperado=0x%08X\n",
                       i, out_buf[i], expected);
            fail++;
        }
    }
    if (fail == 0) printf("\n[OK] %d samples correctas (+0x%X)\n", NUM_SAMPLES, ADD_VALUE);
    else           printf("\n[FAIL] %d/%d\n", fail, NUM_SAMPLES);
    ret = fail ? 1 : 0;

cleanup:
    munmap((void*)dma, dma_size);
    munmap((void*)adder, adder_size);
    munmap(buf, udma_size);
    close(fd_dma); close(fd_adder); close(fd_udma);
    close(fd_irq_mm2s); close(fd_irq_s2mm);
    return ret;
}
