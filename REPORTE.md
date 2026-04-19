# REPORTE FINAL — PetaLinux + Kria KR260

Estado de todo lo que hemos construido y cómo usarlo.
Fecha de generación: 2026-04-19

---

## 🚀 INICIO RÁPIDO (cuando vuelvas del almuerzo)

**Todo empaquetado en un tarball:** `E:\petalinux-instalado\deploy_kr260\kria_kit.tar.gz` (245 KB)

### 3 pasos:

```powershell
# 1. Copiar a la Kria
scp -J rubme@100.127.187.82 "E:\petalinux-instalado\deploy_kr260\kria_kit.tar.gz" ubuntu@192.168.1.150:/tmp/

# 2. SSH y entrar
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150
```

```bash
# 3. En la Kria:
cd /tmp && tar xzf kria_kit.tar.gz && bash setup_kria.sh && bash smoke_test.sh
```

**NOTA:** `setup_kria.sh` pide password UNA VEZ (para configurar NOPASSWD sudo para xmutil).

---

## ✅ Datos ya verificados en la Kria

- Hostname: `kria`
- OS: Ubuntu 22.04.4 LTS (Certified Ubuntu for Kria)
- Kernel: `5.15.0-1027-xilinx-zynqmp`
- Kernel cmdline: `cma=1000M uio_pdrv_genirq.of_id=generic-uio ...`
- **CMA:** 1024 MB reservados, ~1020 MB libres → listo para DMA grandes
- **UIO auto-binding:** activo (`uio_pdrv_genirq` cargado y configurado)
- FPGA state: `operating` (con firmware `k26-starter-kits` por defecto)
- UIOs actuales (antes de cargar stream_adder): uio0-3 **todos `axi-pmon`** (AXI Performance Monitors del PS)
- Ubuntu user: en grupos `sudo, adm, dialout, video, plugdev, netdev, lxd`
- xmutil: `/usr/bin/xmutil` (requiere sudo — configurar NOPASSWD con setup_kria.sh)
- dfx-mgr socket: `/tmp/dfx-mgrd.socket` (activo)

**Tarball ya desplegado y extraído en `/tmp/`:**
```
/tmp/stream_adder_app/
/tmp/tests/
/tmp/setup_kria.sh
/tmp/smoke_test.sh
/tmp/install_udmabuf.sh
/tmp/diagnose_kria.sh
/tmp/README.md
```
Y los tests C ya están **compilados**:
```
/tmp/tests/test_stream_adder    ← ejecutable
/tmp/tests/test_dma_regs        ← ejecutable
/tmp/tests/test_dma_full        ← ejecutable
```

---

## 🎯 ONE-LINER: primera demo completa desde cero

Con el tarball ya en `/tmp/`:
```bash
cd /tmp && bash setup_kria.sh && bash smoke_test.sh
```

**Lo que hace en ~30 segundos:**
1. Configura NOPASSWD para xmutil (pide tu password UNA VEZ)
2. Copia firmware a `/lib/firmware/xilinx/stream_adder/`
3. Descarga el k26-starter-kits actual
4. Carga `stream_adder` accelerator
5. Verifica `fpga0 state = operating`
6. Compila tests si no están
7. Corre `test_stream_adder` (AXI-Lite)
8. Corre `test_dma_regs` (DMA control regs)
9. Devmem roundtrip test

**Resultado esperado:** todos `[OK]`, 0 fails.

---

## 1. Resumen ejecutivo

| Área | Estado | Comentario |
|---|---|---|
| WSL2 + Ubuntu 22.04 en E:\ | ✅ OK | Migrada tras reinicio de Windows |
| PetaLinux 2025.2 (aarch64) | ✅ OK | Instalado en `/home/jce03/petalinux-instalado/` |
| Proyecto P_01 (stream_adder) | ✅ OK | Compilado, BOOT.BIN + image.ub generados |
| Vivado build stream_adder | ✅ OK | Bitstream + XSA generados (~45 min) |
| Accelerator package | ✅ OK | `.bit.bin`, `.dtbo`, `shell.json` en `deploy_kr260\stream_adder_app\` |
| SSH Windows → desktop-6j3hg3r | ✅ OK | Vía Tailscale + ProxyJump |
| SSH → Kria `ubuntu@192.168.1.150` | ⚠️ Inestable | Conecta pero se corta cada pocos min |
| Carga `xmutil loadapp stream_adder` | ⏸ Pendiente | Requiere sudo (ver sección 4) |
| Tests AXI-Lite stream_adder | ⏸ Pendiente | Scripts listos (`tests/`) |

---

## 2. Ruta de archivos

### En tu PC (Windows)

```
E:\petalinux-instalado\
├── P_3_stream_adder\            ← proyecto ORIGINAL (ZedBoard, no tocar)
├── P_3_stream_adder_kr260\      ← proyecto MIGRADO a KR260
│   ├── src\                     ← VHDL (stream_adder, axi_lite_cfg, HsSkidBuf_dest)
│   ├── build_kr260.tcl          ← script Vivado
│   ├── vivado_project\          ← proyecto Vivado (re-generado en cada build)
│   └── design_1_wrapper.xsa     ← XSA exportado (1.5 MB)
├── P_02_simple_axi_reg\         ← proyecto auxiliar (4 registros AXI-Lite)
│   └── src\simple_axi_reg.vhd
├── deploy_kr260\                ← ARCHIVOS PARA LA PLACA
│   ├── BOOT.BIN                 ← (no necesario, la placa usa imagen genérica)
│   ├── image.ub                 ← (no necesario, ídem)
│   ├── stream_adder_app\        ← ★ ACCELERATOR PACKAGE
│   │   ├── stream_adder.bit.bin  (7.5 MB)  ← bitstream
│   │   ├── stream_adder.dtbo     (2.3 KB)  ← device tree overlay
│   │   └── shell.json                       ← metadata XRT
│   └── tests\
│       ├── test_stream_adder.py  ← test Python (mmap /dev/mem)
│       └── test_stream_adder.c   ← test C equivalente
├── build_accel_package.sh       ← script que regenera el package
└── pipeline_nocturno.sh         ← ya ejecutado (build PetaLinux overnight)
```

### En WSL2 (Ubuntu)

```
/home/jce03/
├── petalinux-instalado/         ← SDK PetaLinux 2025.2
├── petalinux-proyectos/
│   └── P_01/                    ← proyecto PetaLinux compilado
│       ├── images/linux/        ← BOOT.BIN, image.ub, system.bit, etc.
│       ├── project-spec/
│       │   ├── hw-description/  ← XSA importado, .bit
│       │   └── meta-user/       ← recipes personalizadas
│       └── components/
│           └── plnx_workspace/device-tree/  ← pl.dtsi generado
├── petalinux-installer.run      ← (lo puedes borrar, ocupa 3 GB)
├── pipeline_nocturno.log        ← log de la build nocturna
└── PIPELINE_DONE.flag           ← flag de OK
```

---

## 3. Diseño hardware stream_adder (KR260)

### Block Design

```
         ┌──────────────┐          
         │  Zynq PS     │          
         │  (K26 SOM)   │          
         │  Cortex-A53  │          
         └┬────────┬────┘          
  M_AXI_HPM0_FPD  S_AXI_HP0_FPD     PL_CLK0 = 100 MHz
   (AXI-Lite)     (AXI-Full)        PL_RESETN
          │          │              IRQ[1:0]
          ▼          ▼                ▲
    ┌──────────┐  ┌──────────┐        │
    │ sc_lite  │  │ sc_mem   │        │ xlconcat
    │ (smart)  │  │ (smart)  │        │ MM2S+S2MM
    └─┬─────┬──┘  └────┬─────┘        │
      │     │          │              │
      ▼     ▼          ▼              │
  DMA   ADDER     ┌────────┐  irqs   │
  Ctl   Ctl       │ AXI    ├─────────┘
                  │ DMA    │
                  │ 7.1    │
                  └┬──────┬┘
                   │ M_AXIS_MM2S
                   ▼      │
              ┌────────┐  │
              │ stream │  │
              │ adder  ├──┘ S_AXIS_S2MM
              └────────┘
```

### Mapa de direcciones

| Bloque | Dirección | Tamaño | Qué hace |
|---|---|---|---|
| `axi_dma_0` | `0xA000_0000` | 64 KB | Control DMA (MM2S + S2MM, scatter-gather off, 32-bit) |
| `stream_adder_0` | `0xA001_0000` | 4 KB | 32 registros AXI-Lite (solo reg0 = `add_value` activo) |

### Interrupts

| IRQ | Origen |
|---|---|
| 89 | DMA MM2S done |
| 90 | DMA S2MM done |

---

## 4. Cómo usar en la KR260 (paso a paso)

### 4.1 — SSH a la Kria

```powershell
# Desde Windows (requires Tailscale activo)
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150
```

O si estás en el PC `desktop-6j3hg3r`:
```cmd
ssh kria
```

### 4.2 — Configurar sudo sin password para xmutil (UNA VEZ)

```bash
# [Bash-KR260] - necesitas introducir tu password de ubuntu esta vez
sudo bash -c 'echo "ubuntu ALL=(ALL) NOPASSWD: /usr/bin/xmutil, /usr/bin/fpgautil, /usr/bin/devmem" > /etc/sudoers.d/pl-nopass'
sudo chmod 440 /etc/sudoers.d/pl-nopass
```

Tras esto, `sudo xmutil` ya no pide password.

### 4.3 — Copiar el paquete a la Kria (si aún no está)

Desde tu PC Windows:
```powershell
scp -J rubme@100.127.187.82 -r "E:\petalinux-instalado\deploy_kr260\stream_adder_app" ubuntu@192.168.1.150:/tmp/
```

### 4.4 — Instalar firmware y cargar

```bash
# [Bash-KR260]
sudo mkdir -p /lib/firmware/xilinx/stream_adder
sudo cp /tmp/stream_adder_app/stream_adder.bit.bin /lib/firmware/xilinx/stream_adder/
sudo cp /tmp/stream_adder_app/stream_adder.dtbo /lib/firmware/xilinx/stream_adder/
sudo cp /tmp/stream_adder_app/shell.json /lib/firmware/xilinx/stream_adder/

# Ver apps disponibles
sudo xmutil listapps

# Descargar cualquier app que esté cargada (ej: k26-starter-kits)
sudo xmutil unloadapp

# Cargar stream_adder
sudo xmutil loadapp stream_adder

# Verificar
cat /sys/class/fpga_manager/fpga0/state       # debe decir: operating
sudo xmutil listapps                          # stream_adder debe salir como "Active"
```

### 4.5 — Alternativa: fpgautil directo (sin xmutil)

Si xmutil da problemas:
```bash
# [Bash-KR260]
sudo fpgautil -b /lib/firmware/xilinx/stream_adder/stream_adder.bit.bin \
              -o /lib/firmware/xilinx/stream_adder/stream_adder.dtbo \
              -f Full
```

### 4.6 — Tests

**Test C rápido (AXI-Lite del stream_adder):**
```bash
# [Bash-KR260]
# Copiar el test.c
scp rubme@desktop-6j3hg3r:/ruta/tests/test_stream_adder.c .  # o copiar desde /tmp
gcc -O2 -Wall -o test_stream_adder test_stream_adder.c
sudo ./test_stream_adder
```

**Test Python:**
```bash
sudo python3 test_stream_adder.py
```

**Test manual con `devmem`:**
```bash
# [Bash-KR260]
# Leer reg0 (debe empezar en 0)
sudo devmem 0xA0010000 32
# Escribir 42
sudo devmem 0xA0010000 32 42
# Leer otra vez (debe decir 0x2A)
sudo devmem 0xA0010000 32
```

---

## 5. Problemas encontrados y soluciones

### 5.1 — mkfs.ext4 sobre NTFS via drvfs (solucionado)

**Problema:** creamos un `.img` ext4 sparse de 150GB en `/mnt/e/`. `mkfs.ext4` quedó en estado D+ (uninterruptible I/O wait) por 40+ min.

**Causa:** mkfs hace muchas pequeñas escrituras que pasan por la capa drvfs de WSL2 hacia NTFS en USB externo, con un overhead brutal.

**Solución adoptada:** instalar PetaLinux directamente en el ext4 nativo del vhdx de WSL, que vive en `E:\wsl-distros\Ubuntu-22.04\ext4.vhdx`. Sigue cumpliendo "todo en E:\" (el vhdx está en E:\) y la velocidad es 10-100x mejor.

### 5.2 — Sintaxis PetaLinux 2025.2 cambiada

Ya no existe `--type project`, ahora se usa subcommand:
- ❌ `petalinux-create --type project --template zynqMP --name X`
- ✅ `petalinux-create project -n X --template zynqMP`

### 5.3 — M_AXI_HPM1_FPD sin clock (solucionado)

El preset de board de KR260 activa automáticamente HPM1_FPD. Si no lo usas, falla la validación.

**Solución:** desactivar explícitamente en el Tcl:
```tcl
CONFIG.PSU__USE__M_AXI_GP1 {0}
```

### 5.4 — SSH a la Kria inestable

El switch intermedio tiene cortes frecuentes. La sesión SSH se cae con "Connection closed by UNKNOWN port 65535" o timeout.

**Workaround:** usar `ssh -o ServerAliveInterval=30 -o ServerAliveCountMax=3` para detectar caídas antes. Reintentar automáticamente.

---

## 6. TODO — pendiente para mañana/después

### 6.1 — Validar AXI-Lite en la placa (próximo paso inmediato)

Ejecutar `test_stream_adder.py` o `test_stream_adder.c` y verificar:
- Escribir `0xDEADBEEF` en `0xA0010000` → leer de vuelta `0xDEADBEEF` ✓

Si esto funciona, el PL está programado y el PS puede hablarle. **Hello world hardware conseguido.**

### 6.2 — Prueba DMA (siguiente hito)

El stream_adder necesita datos por AXI-Stream. Opciones:

**A) Módulo kernel xilinx_axidmatest (built-in):**
```bash
sudo modprobe xilinx_axidmatest
# Ver logs
dmesg | grep -i dma
```
Suele autogenerar un test de loopback si el DMA está en device tree.

**B) Driver custom "dma-proxy":**
Hay ejemplos en Xilinx wiki que crean un char device y permiten a userspace disparar transferencias DMA:
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842041/Linux+DMA+From+User+Space

**C) PYNQ:**
```bash
sudo pip3 install pynq
# Python:
from pynq import Overlay, allocate
ov = Overlay("/lib/firmware/xilinx/stream_adder/stream_adder.bit")
# ...
```
Pero pynq espera un archivo `.hwh` junto al `.bit`. Tendríamos que generarlo.

### 6.3 — Reservar memoria DMA via device tree

**Nota importante:** `reserved-memory` solo se puede declarar al boot, NO en un overlay runtime. Las opciones son:

- **CMA (Contiguous Memory Allocator):** habilitado por defecto en Ubuntu Kria. Tamaño configurable via `cma=` en kernel cmdline. Verificar:
  ```bash
  cat /proc/meminfo | grep -i cma
  grep cma /proc/cmdline
  ```
  El driver xilinx axi-dma usa CMA automáticamente.

- **memory-region en el overlay:** referencia una región ya reservada. Nuestro overlay no la necesita — el driver axi-dma gestiona memoria vía CMA/dma-coherent.

### 6.4 — Empaquetar hello-kr260 binario para la placa

El `hello-kr260` C está compilado en la imagen PetaLinux (`image.ub`), no en Ubuntu Kria. Si quieres usarlo sobre la imagen Ubuntu:
```bash
# Compilar nativo en la placa
scp /home/jce03/.../hello-kr260.c ubuntu@kria:/tmp/
ssh kria "gcc -o hello-kr260 /tmp/hello-kr260.c && ./hello-kr260"
```

---

## 7. Comandos de emergencia / debug

### Ver estado FPGA
```bash
cat /sys/class/fpga_manager/fpga0/state    # "operating" = PL programado
cat /sys/class/fpga_manager/fpga0/name     # nombre del manager
```

### Ver device tree runtime
```bash
ls /sys/firmware/devicetree/base/amba/
# Debería aparecer: dma@a0000000, stream_adder@a0010000
```

### Ver UIO devices (si usamos generic-uio)
```bash
ls /dev/uio*
cat /sys/class/uio/uio0/name
cat /sys/class/uio/uio0/maps/map0/size
```

### Ver dmesg relevante
```bash
dmesg | grep -iE "fpga|dma|stream|xlnx|zocl"
```

### Resetear FPGA / descargar app
```bash
sudo xmutil unloadapp
# El PL queda con el bitstream "base" (shell de Kria)
```

---

## 8. Rutas relativas PC→Kria rápidas

```powershell
# Copiar un archivo a la Kria
scp -J rubme@100.127.187.82 "E:\archivo.txt" ubuntu@192.168.1.150:/tmp/

# Ejecutar un comando
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150 "comando"

# Copiar desde Kria a tu PC
scp -J rubme@100.127.187.82 ubuntu@192.168.1.150:/tmp/output.txt "E:\"
```

---

## 9. Regenerar artefactos desde cero

### Rebuild bitstream (Vivado)
```powershell
cd E:\petalinux-instalado\P_3_stream_adder_kr260
E:\vivado-instalado\2025.2.1\Vivado\bin\vivado.bat -mode batch -source build_kr260.tcl
```

### Rebuild PetaLinux
```bash
# [Bash-WSL2]
cd /home/jce03/petalinux-proyectos/P_01
source /home/jce03/petalinux-instalado/settings.sh
petalinux-config --silentconfig --get-hw-description ./design_1_wrapper.xsa
petalinux-build
petalinux-package boot --u-boot --fpga --force
```

### Regenerar accelerator package
```bash
# [Bash-WSL2]
bash /mnt/e/petalinux-instalado/build_accel_package.sh
```
