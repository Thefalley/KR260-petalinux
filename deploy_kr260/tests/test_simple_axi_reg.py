#!/usr/bin/env python3
"""
test_simple_axi_reg.py - Test del IP simple_axi_reg (P_02)

Si has cargado el bitstream de P_02, el registro en offset 0x0C
siempre devuelve 0xBADC0DE5.

Uso: sudo python3 test_simple_axi_reg.py
"""

import mmap
import os
import struct
import sys

BASE = 0xA0000000
SIZE = 0x1000
ID_EXPECT = 0xBADC0DE5


def main():
    if os.geteuid() != 0:
        sys.exit("Requiere sudo")

    with open("/dev/mem", "r+b") as f:
        mem = mmap.mmap(f.fileno(), SIZE, offset=BASE)

    pass_n = fail_n = 0

    # Test ID
    mem.seek(0x0C)
    id_val = struct.unpack("<I", mem.read(4))[0]
    status = "OK" if id_val == ID_EXPECT else "FAIL"
    print(f"reg3 (ID) = 0x{id_val:08X} (esperado 0x{ID_EXPECT:08X}) [{status}]")
    (pass_n := pass_n + 1) if id_val == ID_EXPECT else (fail_n := fail_n + 1)

    # Test RW en reg0, reg1, reg2
    vals = [0x11111111, 0xAAAAAAAA, 0x00000000, 0xFFFFFFFF]
    for r in range(3):
        for v in vals:
            mem.seek(r * 4)
            mem.write(struct.pack("<I", v))
            mem.seek(r * 4)
            got = struct.unpack("<I", mem.read(4))[0]
            if got == v:
                pass_n += 1
            else:
                print(f"  reg{r} write 0x{v:08X} read 0x{got:08X} [FAIL]")
                fail_n += 1

    # Test reg3 es RO
    mem.seek(0x0C)
    mem.write(struct.pack("<I", 0xDEADBEEF))
    mem.seek(0x0C)
    after = struct.unpack("<I", mem.read(4))[0]
    if after == ID_EXPECT:
        print("reg3 RO intacto tras escritura [OK]")
        pass_n += 1
    else:
        print(f"reg3 cambio a 0x{after:08X} [FAIL]")
        fail_n += 1

    print(f"\n{pass_n} OK, {fail_n} FAIL")
    mem.close()
    sys.exit(0 if fail_n == 0 else 1)


if __name__ == "__main__":
    main()
