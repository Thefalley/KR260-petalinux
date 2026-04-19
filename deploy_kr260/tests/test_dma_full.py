#!/usr/bin/env python3
"""
test_dma_full.py - Test DMA MM2S -> stream_adder -> S2MM en KR260

Usa /dev/mem (AXI-Lite control) + /dev/udmabuf0 (buffer contiguo CMA).

Requiere previo:
  1. xmutil loadapp stream_adder
  2. sudo modprobe u-dma-buf udmabufs=udmabuf0=2097152

Ejecutar:
  sudo python3 test_dma_full.py
"""

import mmap
import os
import struct
import sys
import time

DMA_BASE    = 0xA0000000
DMA_SIZE    = 0x10000
ADDER_BASE  = 0xA0010000
ADDER_SIZE  = 0x1000

# Registros DMA (PG021)
REG_MM2S_DMACR   = 0x00
REG_MM2S_DMASR   = 0x04
REG_MM2S_SA      = 0x18
REG_MM2S_LENGTH  = 0x28
REG_S2MM_DMACR   = 0x30
REG_S2MM_DMASR   = 0x34
REG_S2MM_DA      = 0x48
REG_S2MM_LENGTH  = 0x58

DMACR_RS    = 0x1
DMACR_RESET = 0x4
DMASR_IDLE  = 0x2
DMASR_ERR   = 0x70

NUM_SAMPLES = 256
XFER_BYTES  = NUM_SAMPLES * 4
BUFFER_SIZE = 2 * 1024 * 1024
MM2S_OFFSET = 0
S2MM_OFFSET = 1 * 1024 * 1024
ADD_VALUE   = 0x100


def rd(mem, off):
    mem.seek(off)
    return struct.unpack("<I", mem.read(4))[0]


def wr(mem, off, val):
    mem.seek(off)
    mem.write(struct.pack("<I", val))


def wait_idle(mem, off, timeout=5.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        sr = rd(mem, off)
        if sr & DMASR_ERR:
            raise RuntimeError(f"DMA error SR=0x{sr:08X}")
        if sr & DMASR_IDLE:
            return
        time.sleep(0.001)
    raise TimeoutError(f"DMA timeout, SR=0x{rd(mem, off):08X}")


def main():
    if os.geteuid() != 0:
        sys.exit("Requiere sudo")

    try:
        phys_addr = int(open("/sys/class/u-dma-buf/udmabuf0/phys_addr").read().strip(), 16)
    except FileNotFoundError:
        sys.exit("/dev/udmabuf0 no existe. Cargar: sudo modprobe u-dma-buf udmabufs=udmabuf0=2097152")

    print(f"udmabuf0 phys = 0x{phys_addr:X}")

    mf = open("/dev/mem", "r+b")
    dma = mmap.mmap(mf.fileno(), DMA_SIZE, offset=DMA_BASE)
    adder = mmap.mmap(mf.fileno(), ADDER_SIZE, offset=ADDER_BASE)

    uf = open("/dev/udmabuf0", "r+b")
    buf = mmap.mmap(uf.fileno(), BUFFER_SIZE)

    # Preparar datos
    for i in range(NUM_SAMPLES):
        buf.seek(MM2S_OFFSET + i*4)
        buf.write(struct.pack("<I", i))
    # Limpiar salida
    buf.seek(S2MM_OFFSET)
    buf.write(b"\xFF" * XFER_BYTES)

    # Configurar add_value
    wr(adder, 0x00, ADD_VALUE)
    print(f"add_value = 0x{ADD_VALUE:X}")

    # Reset DMA
    wr(dma, REG_MM2S_DMACR, DMACR_RESET)
    while rd(dma, REG_MM2S_DMACR) & DMACR_RESET: time.sleep(0.001)
    wr(dma, REG_S2MM_DMACR, DMACR_RESET)
    while rd(dma, REG_S2MM_DMACR) & DMACR_RESET: time.sleep(0.001)

    # Arrancar RS
    wr(dma, REG_MM2S_DMACR, DMACR_RS)
    wr(dma, REG_S2MM_DMACR, DMACR_RS)

    # Programar S2MM primero (receptor)
    wr(dma, REG_S2MM_DA, phys_addr + S2MM_OFFSET)
    wr(dma, REG_S2MM_LENGTH, XFER_BYTES)

    # Programar y lanzar MM2S
    wr(dma, REG_MM2S_SA, phys_addr + MM2S_OFFSET)
    wr(dma, REG_MM2S_LENGTH, XFER_BYTES)  # escritura de LENGTH = start

    print("Transferencia en curso...")
    wait_idle(dma, REG_MM2S_DMASR)
    print("MM2S OK")
    wait_idle(dma, REG_S2MM_DMASR)
    print("S2MM OK")

    # Verificar
    fail = 0
    for i in range(NUM_SAMPLES):
        buf.seek(S2MM_OFFSET + i*4)
        got = struct.unpack("<I", buf.read(4))[0]
        exp = (i + ADD_VALUE) & 0xFFFFFFFF
        if got != exp:
            if fail < 10:
                print(f"[FAIL] out[{i}] = 0x{got:08X} (esperado 0x{exp:08X})")
            fail += 1

    if fail == 0:
        print(f"\n[OK] {NUM_SAMPLES} samples correctas")
    else:
        print(f"\n[FAIL] {fail}/{NUM_SAMPLES}")

    buf.close(); uf.close()
    dma.close(); adder.close(); mf.close()
    sys.exit(0 if fail == 0 else 1)


if __name__ == "__main__":
    main()
