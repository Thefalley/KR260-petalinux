# Makefile - tests stream_adder KR260
# Ejecutar en la Kria: make

CC      = gcc
CFLAGS  = -O2 -Wall -Wextra
TARGETS = test_stream_adder test_dma_regs test_dma_full

.PHONY: all clean run-basic run-dma install

all: $(TARGETS)

test_stream_adder: test_stream_adder.c
	$(CC) $(CFLAGS) -o $@ $<

test_dma_regs: test_dma_regs.c
	$(CC) $(CFLAGS) -o $@ $<

test_dma_full: test_dma_full.c
	$(CC) $(CFLAGS) -o $@ $<

run-basic: test_stream_adder test_dma_regs
	@echo "=== Test AXI-Lite stream_adder ==="
	sudo ./test_stream_adder
	@echo ""
	@echo "=== Test registros DMA ==="
	sudo ./test_dma_regs

run-dma: test_dma_full
	@echo "=== Test DMA completo ==="
	@test -c /dev/udmabuf0 || (echo "ERROR: /dev/udmabuf0 no existe. Cargar: sudo modprobe u-dma-buf udmabufs=udmabuf0=2097152" && false)
	sudo ./test_dma_full

install: $(TARGETS)
	sudo cp $(TARGETS) /usr/local/bin/

clean:
	rm -f $(TARGETS)
