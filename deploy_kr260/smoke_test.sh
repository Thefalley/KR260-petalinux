#!/bin/bash
# ================================================================
# smoke_test.sh - Corre todos los tests del stream_adder en la KRIA
# ================================================================
set +e  # no abortar en primer fallo, queremos ver todo

cd "$(dirname "$0")"

PASS=0
FAIL=0

divider() { echo ""; echo "=================================================="; }

divider
echo "[SMOKE 1/5] Estado FPGA y accelerator"
divider
STATE=$(cat /sys/class/fpga_manager/fpga0/state 2>/dev/null)
echo "fpga0 state: $STATE"
if [ "$STATE" = "operating" ]; then
    echo "[OK] FPGA programado"
    PASS=$((PASS+1))
else
    echo "[FAIL] FPGA NO programado - ejecuta setup_kria.sh primero"
    FAIL=$((FAIL+1))
fi

divider
echo "[SMOKE 2/5] Device tree runtime"
divider
for node in "amba_pl" "dma@a0000000" "stream_adder@a0010000"; do
    if [ -d "/sys/firmware/devicetree/base/amba/$node" ] || \
       [ -d "/sys/firmware/devicetree/base/amba-pl@0/$node" ]; then
        echo "[OK] Nodo DT $node presente"
        PASS=$((PASS+1))
    else
        # Buscar recursivamente
        FOUND=$(find /sys/firmware/devicetree/base/ -maxdepth 3 -name "$node" 2>/dev/null | head -1)
        if [ -n "$FOUND" ]; then
            echo "[OK] Nodo DT $node en $FOUND"
            PASS=$((PASS+1))
        else
            echo "[FAIL] Nodo DT $node no encontrado"
            FAIL=$((FAIL+1))
        fi
    fi
done

divider
echo "[SMOKE 3/5] Test AXI-Lite stream_adder (reg0 add_value)"
divider
if [ -f test_stream_adder ]; then
    ./test_stream_adder 2>&1
    [ $? -eq 0 ] && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
else
    echo "Compilando test_stream_adder..."
    gcc -O2 -Wall -o test_stream_adder test_stream_adder.c && sudo ./test_stream_adder
    [ $? -eq 0 ] && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
fi

divider
echo "[SMOKE 4/5] Test registros AXI DMA"
divider
if [ -f test_dma_regs ]; then
    sudo ./test_dma_regs 2>&1
    [ $? -eq 0 ] && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
else
    echo "Compilando test_dma_regs..."
    gcc -O2 -Wall -o test_dma_regs test_dma_regs.c && sudo ./test_dma_regs
    [ $? -eq 0 ] && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
fi

divider
echo "[SMOKE 5/5] devmem manual — escritura/lectura directa"
divider
echo "Escribiendo 0xDEADBEEF en 0xA0010000..."
sudo devmem 0xA0010000 32 0xDEADBEEF
VAL=$(sudo devmem 0xA0010000 32)
echo "Leido: $VAL"
if [ "$VAL" = "0xDEADBEEF" ]; then
    echo "[OK] devmem roundtrip"
    PASS=$((PASS+1))
else
    echo "[FAIL] esperaba 0xDEADBEEF, leido $VAL"
    FAIL=$((FAIL+1))
fi

divider
echo "RESULTADO SMOKE TEST: $PASS OK, $FAIL FAIL"
divider

exit $FAIL
