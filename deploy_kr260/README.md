# deploy_kr260 — kit para llevar a la Kria KR260

## Contenido del kit

```
deploy_kr260/
├── stream_adder_app/         ★ Firmware + overlay (copiar a /lib/firmware/xilinx/)
│   ├── stream_adder.bit.bin   (7.5 MB bitstream)
│   ├── stream_adder.dtbo      (overlay device tree)
│   └── shell.json             (metadata XRT)
├── tests/                     ★ Tests C + Python + Makefile
│   ├── test_stream_adder.c / .py   (AXI-Lite registro)
│   ├── test_dma_regs.c              (registros AXI DMA)
│   ├── test_dma_full.c / .py        (transferencia DMA completa)
│   └── Makefile
├── setup_kria.sh             Una vez — instala firmware, sudoers, carga app
├── smoke_test.sh             Corre todos los tests
├── install_udmabuf.sh        Instala u-dma-buf para tests DMA userspace
└── BOOT.BIN, image.ub, rootfs.tar.gz, ...    (imagen PetaLinux completa — no necesario con Kria-Ubuntu)
```

## Flujo de primera vez (5 min)

### 1. Desde tu PC Windows, copia el kit a la Kria
```powershell
scp -J rubme@100.127.187.82 -r "E:\petalinux-instalado\deploy_kr260\stream_adder_app" "E:\petalinux-instalado\deploy_kr260\tests" "E:\petalinux-instalado\deploy_kr260\setup_kria.sh" "E:\petalinux-instalado\deploy_kr260\smoke_test.sh" ubuntu@192.168.1.150:/tmp/
```

### 2. SSH a la Kria
```powershell
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150
```

### 3. En la Kria, ejecuta (una vez)
```bash
cd /tmp
bash setup_kria.sh            # pide password de ubuntu; configura NOPASSWD y carga firmware
```

### 4. Corre los tests básicos
```bash
cd /tmp/tests
make
bash ../smoke_test.sh         # tests AXI-Lite + DMA registros
```

### 5. Tests DMA completos (opcional, requiere un paso más)
```bash
cd /tmp
bash install_udmabuf.sh       # compila e instala u-dma-buf
cd tests
make test_dma_full
sudo ./test_dma_full          # transferencia real PS -> stream_adder -> PS
```

## Qué valida cada test

| Test | Qué prueba | Tiempo |
|---|---|---|
| `test_stream_adder.c` | AXI-Lite del stream_adder (reg0 RW) | 1 s |
| `test_dma_regs.c` | Registros AXI DMA accesibles + reset | 1 s |
| `test_dma_full.c` | Flujo DMA completo MM2S → PL → S2MM | 1-2 s |
| `smoke_test.sh` | Todo encadenado | 10 s |

## Si algo falla

- **FPGA state no es "operating"** → `sudo xmutil loadapp stream_adder`
- **/dev/udmabuf0 no existe** → correr `install_udmabuf.sh`
- **test_stream_adder falla con 0xFFFFFFFF siempre** → bitstream no cargado
- **DMA TIMEOUT** → revisar interrupts en dmesg: `sudo dmesg | grep -i dma | tail`
