#!/usr/bin/env python3
"""
test_stream_adder.py - Test completo del IP stream_adder en KR260

Requiere:
  - xmutil loadapp stream_adder (ya ejecutado)
  - sudo (para acceso a /dev/mem)

Este script:
  1. Abre /dev/mem y mapea el bloque AXI-Lite del stream_adder
  2. Escribe distintos valores en el registro add_value (reg0)
  3. Los lee y verifica que se escribieron correctamente
  4. Prueba casos borde (0, 0xFFFFFFFF, valores intermedios)

Uso:
  sudo python3 test_stream_adder.py
"""

import mmap
import os
import struct
import sys

# Direcciones del AXI-Lite del stream_adder
STREAM_ADDER_BASE = 0xA0010000
STREAM_ADDER_SIZE = 0x1000   # 4 KB

# Registros (offsets desde base)
REG_ADD_VALUE = 0x00    # reg0 - valor a sumar al stream


def test_register_rw(mem, name, offset, test_values):
    """Prueba escribir y leer un registro."""
    print(f"\n=== Test registro {name} (offset 0x{offset:02X}) ===")
    passed = 0
    failed = 0

    for val in test_values:
        # Escribir
        mem.seek(offset)
        mem.write(struct.pack("<I", val))

        # Leer
        mem.seek(offset)
        read_back = struct.unpack("<I", mem.read(4))[0]

        status = "OK" if read_back == val else "FAIL"
        if read_back == val:
            passed += 1
        else:
            failed += 1

        print(f"  write 0x{val:08X} -> read 0x{read_back:08X}  [{status}]")

    return passed, failed


def main():
    if os.geteuid() != 0:
        print("ERROR: este script requiere sudo (acceso a /dev/mem)")
        sys.exit(1)

    try:
        with open("/dev/mem", "r+b") as f:
            mem = mmap.mmap(
                f.fileno(),
                STREAM_ADDER_SIZE,
                mmap.MAP_SHARED,
                mmap.PROT_READ | mmap.PROT_WRITE,
                offset=STREAM_ADDER_BASE,
            )
    except Exception as e:
        print(f"ERROR abriendo /dev/mem: {e}")
        print("  -> Comprobar que xmutil loadapp stream_adder ejecuto OK")
        print("  -> cat /sys/class/fpga_manager/fpga0/state debe decir 'operating'")
        sys.exit(1)

    print(f"AXI-Lite mapeado en 0x{STREAM_ADDER_BASE:08X} - 0x{STREAM_ADDER_BASE+STREAM_ADDER_SIZE:08X}")

    # Valores a probar
    test_vals = [
        0x00000000,
        0x00000001,
        0x0000000F,
        0x000000FF,
        0x0000FFFF,
        0x12345678,
        0xDEADBEEF,
        0xFFFFFFFF,
    ]

    passed, failed = test_register_rw(mem, "add_value", REG_ADD_VALUE, test_vals)

    print(f"\n{'='*50}")
    print(f"RESULTADO: {passed} OK, {failed} FAIL de {len(test_vals)} pruebas")
    print(f"{'='*50}")

    mem.close()

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
