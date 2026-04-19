#!/bin/bash
# ================================================================
# setup_kria.sh - Configuracion inicial EN LA KRIA (uno vez)
# Ejecutar: bash setup_kria.sh
# ================================================================
set -e

echo "=== [1/4] Verificando sistema ==="
if [ "$(hostname)" != "kria" ]; then
    echo "ADVERTENCIA: hostname no es 'kria' (es '$(hostname)')"
fi
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME)"
echo "Kernel: $(uname -r)"

echo ""
echo "=== [2/4] NOPASSWD para xmutil/fpgautil/devmem ==="
if [ -f /etc/sudoers.d/pl-nopass ]; then
    echo "Ya configurado"
else
    sudo bash -c 'cat > /etc/sudoers.d/pl-nopass << EOF
ubuntu ALL=(ALL) NOPASSWD: /usr/bin/xmutil, /usr/bin/fpgautil, /usr/bin/devmem
EOF'
    sudo chmod 440 /etc/sudoers.d/pl-nopass
    echo "OK - sudoers configurado"
fi

echo ""
echo "=== [3/4] Instalando firmware stream_adder ==="
if [ ! -d /tmp/stream_adder_app ]; then
    echo "ERROR: falta /tmp/stream_adder_app — hacer scp primero"
    exit 1
fi
sudo mkdir -p /lib/firmware/xilinx/stream_adder
sudo cp /tmp/stream_adder_app/stream_adder.bit.bin /lib/firmware/xilinx/stream_adder/
sudo cp /tmp/stream_adder_app/stream_adder.dtbo    /lib/firmware/xilinx/stream_adder/
sudo cp /tmp/stream_adder_app/shell.json           /lib/firmware/xilinx/stream_adder/
sudo chmod 644 /lib/firmware/xilinx/stream_adder/*
ls -la /lib/firmware/xilinx/stream_adder/

echo ""
echo "=== [4/4] Cargando accelerator ==="
sudo xmutil unloadapp 2>/dev/null || echo "(no app cargada previamente)"
sleep 1
sudo xmutil loadapp stream_adder
sleep 1
echo ""
echo "FPGA state: $(cat /sys/class/fpga_manager/fpga0/state)"
echo ""
sudo xmutil listapps

echo ""
echo "=========================================================="
echo "  Setup completo. Siguiente: bash smoke_test.sh"
echo "=========================================================="
