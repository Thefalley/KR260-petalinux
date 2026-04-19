#!/bin/bash
# ================================================================
# diagnose_kria.sh - Dump completo del estado de la Kria
# Util para compartir logs si algo falla
# ================================================================

OUT="${1:-/tmp/kria_diag_$(date +%Y%m%d_%H%M%S).txt}"

{
echo "=== Diagnostico Kria KR260 - $(date) ==="
echo ""
echo "=== Sistema ==="
echo "hostname: $(hostname)"
echo "kernel: $(uname -r)"
echo "os: $(cat /etc/os-release | grep PRETTY_NAME)"
echo "uptime: $(uptime -p)"
echo ""

echo "=== Kernel cmdline ==="
cat /proc/cmdline
echo ""

echo "=== CMA ==="
grep -i cma /proc/meminfo
echo ""

echo "=== Memoria ==="
free -h | head -2
echo ""

echo "=== FPGA Manager ==="
echo "state:     $(cat /sys/class/fpga_manager/fpga0/state 2>/dev/null)"
echo "name:      $(cat /sys/class/fpga_manager/fpga0/name 2>/dev/null)"
echo ""

echo "=== Apps instaladas ==="
ls -la /lib/firmware/xilinx/ 2>&1
echo ""
echo "--- stream_adder files ---"
ls -la /lib/firmware/xilinx/stream_adder/ 2>&1
echo ""

echo "=== xmutil listapps (requiere sudo) ==="
sudo -n xmutil listapps 2>&1 | head -30
echo ""

echo "=== Device Tree runtime ==="
echo "-- Hay nodos AXI en PL? --"
find /sys/firmware/devicetree/base -maxdepth 4 -name "dma@*" -o -name "stream_adder*" -o -name "amba_pl*" 2>/dev/null
echo ""
echo "-- compatible strings --"
for f in $(find /sys/firmware/devicetree/base -maxdepth 5 -name compatible 2>/dev/null | head -20); do
    echo "$f: $(cat $f | tr -d '\0' 2>&1)"
done | head -30
echo ""

echo "=== UIO devices ==="
ls -la /dev/uio* 2>&1
for u in /sys/class/uio/uio*; do
    if [ -d "$u" ]; then
        echo "--- $u ---"
        echo "name:     $(cat $u/name 2>&1)"
        echo "version:  $(cat $u/version 2>&1)"
        for m in $u/maps/map*; do
            if [ -d "$m" ]; then
                echo "$(basename $m): addr=$(cat $m/addr 2>&1)  size=$(cat $m/size 2>&1)  name=$(cat $m/name 2>&1)"
            fi
        done
    fi
done
echo ""

echo "=== udmabuf ==="
ls -la /dev/udmabuf* 2>&1
for u in /sys/class/u-dma-buf/udmabuf*; do
    if [ -d "$u" ]; then
        echo "$u: phys=$(cat $u/phys_addr 2>&1)  size=$(cat $u/size 2>&1)"
    fi
done
echo ""

echo "=== DMA Engine ==="
ls -la /sys/class/dma/ 2>&1
cat /sys/kernel/debug/dmaengine/summary 2>&1 | head -20
echo ""

echo "=== Modulos Xilinx cargados ==="
lsmod | grep -iE "xilinx|xlnx|axi|dma|uio|zocl"
echo ""

echo "=== Interrupts asignados PL ==="
grep -iE "xilinx|axi|dma" /proc/interrupts
echo ""

echo "=== dmesg filtrado (ultimo boot) ==="
dmesg | grep -iE "fpga|dma|stream|xlnx|uio|zynqmp|cma" | tail -40
echo ""

echo "=== Red ==="
ip -4 addr | grep -E "inet |eth|wlan"
echo ""

echo "=== Disco ==="
df -h / /boot 2>&1 | head -5
echo ""

echo "=== Fin diagnostico ==="
} | tee "$OUT"

echo ""
echo "Guardado en: $OUT"
echo "Pasalo al amigo asi: scp $OUT tu_pc:/ruta/"
