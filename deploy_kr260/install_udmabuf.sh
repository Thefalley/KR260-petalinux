#!/bin/bash
# ================================================================
# install_udmabuf.sh - Instalar u-dma-buf en Kria Ubuntu
# ================================================================
# u-dma-buf (ikwzm/udmabuf) crea /dev/udmabuf0 que userspace puede
# mmap. El buffer sale de CMA, físicamente contiguo, perfecto para
# DMA desde userspace sin driver kernel.
#
# Ejecutar EN LA KRIA: bash install_udmabuf.sh
# ================================================================
set -e

echo "=== [1/4] Dependencias ==="
sudo apt update -qq
sudo apt install -y build-essential linux-headers-$(uname -r) git

echo ""
echo "=== [2/4] Clonar u-dma-buf ==="
cd ~
if [ ! -d u-dma-buf ]; then
    git clone https://github.com/ikwzm/u-dma-buf.git
fi
cd u-dma-buf

echo ""
echo "=== [3/4] Compilar modulo ==="
make

echo ""
echo "=== [4/4] Cargar con buffer 2MB ==="
sudo cp u-dma-buf.ko /lib/modules/$(uname -r)/extra/ 2>/dev/null || \
    sudo mkdir -p /lib/modules/$(uname -r)/extra && \
    sudo cp u-dma-buf.ko /lib/modules/$(uname -r)/extra/
sudo depmod

# Cargar con 2MB
sudo modprobe u-dma-buf udmabufs=udmabuf0=2097152 2>/dev/null || \
    sudo insmod u-dma-buf.ko udmabufs=udmabuf0=2097152

echo ""
echo "=== Verificacion ==="
ls -la /dev/udmabuf0 2>&1
cat /sys/class/u-dma-buf/udmabuf0/phys_addr 2>&1
cat /sys/class/u-dma-buf/udmabuf0/size 2>&1
echo ""

# Permitir acceso sin sudo (opcional)
sudo chmod 666 /dev/udmabuf0

# Cargar al boot
if ! grep -q "u-dma-buf" /etc/modules-load.d/*.conf 2>/dev/null; then
    echo "u-dma-buf" | sudo tee /etc/modules-load.d/u-dma-buf.conf
    echo "options u-dma-buf udmabufs=udmabuf0=2097152" | sudo tee /etc/modprobe.d/u-dma-buf.conf
fi

echo "=========================================================="
echo "  u-dma-buf instalado. /dev/udmabuf0 disponible."
echo "  Test: sudo ./test_dma_full"
echo "=========================================================="
