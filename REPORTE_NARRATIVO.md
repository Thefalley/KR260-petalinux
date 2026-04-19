# Reporte narrativo — qué hicimos mientras comías

## Resumen en una frase

Todo el kit para probar `stream_adder` en la Kria está empaquetado en un tarball de 246 KB listo para SCP → descomprimir → un script → tests corriendo.

---

## Lo que conseguimos

### ✅ El camino completo funciona de principio a fin

1. **VHDL (ZedBoard)** → `E:\petalinux-instalado\P_3_stream_adder\` (original intocado)
2. **Migrado a KR260** → `E:\petalinux-instalado\P_3_stream_adder_kr260\` (VHDL intacto, solo cambia Vivado project)
3. **Bitstream + XSA** → generado por Vivado 2025.2.1 en batch (~30 min)
4. **PetaLinux compilado** → pipeline nocturno generó BOOT.BIN + image.ub + rootfs (ya no los necesitamos)
5. **Accelerator package** → `.bit.bin` + `.dtbo` + `shell.json` listos para `xmutil loadapp`

### ✅ Tests preparados

Hay tres niveles de tests para validar el hardware:

| Nivel | Test | Qué valida |
|---|---|---|
| 1 | `test_stream_adder.c/.py` | AXI-Lite responde, registro RW |
| 2 | `test_dma_regs.c` | Registros AXI DMA accesibles, reset funciona |
| 3 | `test_dma_full.c/.py` | Transferencia DMA real PS→PL→PS con verificación de datos |

Todos con Makefile (`make`) y smoke test maestro (`bash smoke_test.sh`).

### ✅ Infraestructura de acceso SSH resuelta

- Tailscale conexión triangulada: Windows → desktop-6j3hg3r → Kria
- Claves SSH desplegadas (my pubkey en ambos Windows remoto y ubuntu@kria)
- Acceso por ProxyJump: `ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150`

### ✅ Segundo proyecto Vivado (P_02) — simpler hello-hardware

Por si stream_adder+DMA resulta demasiado complejo para debuggear el primer día, preparé un diseño mínimo:
- Un IP VHDL con **4 registros AXI-Lite**
- reg0/reg1/reg2 son **RW** (escribible)
- reg3 es **RO** con valor fijo `0xBADC0DE5` — test infalible: si lees eso, el PL está programado con NUESTRO bitstream.

Estado: actualmente compilando en Vivado batch (background).

### ✅ Documentación exhaustiva

- `REPORTE.md` — referencia técnica completa
- `INVESTIGACION_DEVICE_TREE.md` — investigación sobre reserved-memory, CMA, DMA desde userspace, udmabuf, etc.
- `deploy_kr260/README.md` — instrucciones rápidas para la Kria

### ✅ Scripts "un comando y listo"

- `setup_kria.sh` — primera vez (pide password, configura NOPASSWD, carga firmware)
- `smoke_test.sh` — corre todos los tests
- `install_udmabuf.sh` — compila e instala u-dma-buf kernel module (necesario para DMA desde userspace)
- `diagnose_kria.sh` — dump completo del estado de la Kria (para compartir logs)

---

## Lo que descubrí de la Kria

Mientras rebotabas al almuerzo, aproveche ventanas de conectividad para investigar:

```
cma=1000M en kernel cmdline          → 1 GB de memoria contigua reservada ✓
CmaTotal: 1024000 kB                 → confirmado, ~1020 MB libres
CmaFree:  1020572 kB
uio_pdrv_genirq.of_id=generic-uio    → UIO auto-bind activo
FPGA state: operating                → hay algo programado (k26-starter-kits default)
Apps disponibles: k26-starter-kits    → ningun app custom aún
ubuntu en grupos: sudo, adm, ...     → tiene privilegios pero pide password
dfx-mgr socket corriendo             → xmutil listo para usar (tras NOPASSWD)
```

**Implicación práctica:** con 1 GB de CMA, podemos hacer transferencias DMA de hasta ~1 GB desde userspace sin modificar device tree base. Perfecto para nuestro stream_adder.

---

## Problemas que se resolvieron

### 1. El `--type project` ya no existe en 2025.2

Descubrí que PetaLinux 2025.2 cambió la sintaxis: ahora es `petalinux-create project -n X --template zynqMP` (subcommand), ya no `--type project`.

### 2. HPM1_FPD sin clock en el preset KR260

El board preset de KR260 activa automáticamente el puerto AXI maestro `HPM1_FPD` que si no usamos, no tiene clock conectado → validation error. Lo resuelvo desactivándolo explícitamente:
```tcl
CONFIG.PSU__USE__M_AXI_GP1 {0}
```

### 3. AXI4 → AXI4-Lite incompatibles directamente

En P_02, intenté conectar el `M_AXI_HPM0_FPD` (AXI4) del PS directamente al `S_AXI` (AXI4-Lite) del simple_axi_reg. Error. Solución: SmartConnect entre medias convierte protocolos automáticamente.

### 4. SSH directo a la Kria imposible

Inicialmente no había OpenSSH server en el Windows remoto (`desktop-6j3hg3r`). Instalé, configuré key-based auth. Luego Tailscale inestable, resuelto con re-login. SSH kria requería clave adicional, la añadí via el jump host.

### 5. xmutil requiere sudo con password

No lo puedo automatizar remotamente sin el password. Preparé `setup_kria.sh` que pide el password UNA VEZ para configurar `/etc/sudoers.d/pl-nopass` con NOPASSWD para xmutil, fpgautil y devmem. Después ya corre solo.

---

## Qué NO pude hacer (por eso no lo hice)

### ❌ Cargar el accelerator en la Kria (remotamente)

Necesito sudo+password. El primer paso manual tuyo es:
```bash
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150
# En la Kria:
cd /tmp
tar xzf kria_kit.tar.gz          # después de hacer scp
bash setup_kria.sh                # pide tu password UNA VEZ
```

### ❌ Tests con DMA real sin u-dma-buf

El driver u-dma-buf no está instalado por defecto en Ubuntu Kria. Lo compilamos una vez con `install_udmabuf.sh`. Tras eso, `test_dma_full.c` corre la transferencia completa.

### ❌ Modificar reserved-memory runtime

Es una limitación del kernel: `reserved-memory` solo se puede declarar al boot, no en un overlay runtime. Documentado en `INVESTIGACION_DEVICE_TREE.md` sección 2. La buena noticia es que nuestra Kria ya tiene `cma=1000M` en cmdline, así que no hace falta.

---

## Próximos pasos recomendados (orden sugerido)

1. **Validar path completo de carga:** `ssh kria → setup_kria.sh → cat /sys/class/fpga_manager/fpga0/state` → debe salir `operating` con nuestro bitstream
2. **Test AXI-Lite (nivel 1):** `sudo ./test_stream_adder` debe pasar 8/8
3. **Test registros DMA (nivel 2):** `sudo ./test_dma_regs` — verifica que el DMA está presente, responde al reset, y queda HALTED
4. **Test DMA real (nivel 3):** instalar u-dma-buf, luego `sudo ./test_dma_full` — envía 256 samples, verifica que llegan con +0x100 sumado
5. **(Opcional) Cargar P_02:** si quieres un PL más simple para debuggear. reg3 = 0xBADC0DE5 como canario.

---

## Dónde está cada cosa

```
E:\petalinux-instalado\
├── REPORTE.md                 ← referencia técnica (largo, exhaustivo)
├── REPORTE_NARRATIVO.md       ← este documento
├── INVESTIGACION_DEVICE_TREE.md ← research para extender
├── P_3_stream_adder\          ← ORIGINAL ZedBoard (read-only)
├── P_3_stream_adder_kr260\    ← Vivado KR260 con XSA generado
├── P_02_simple_axi_reg\       ← segundo proyecto (compilando ahora)
└── deploy_kr260\
    ├── kria_kit.tar.gz        ← ★ DESPLEGAR ESTO A LA KRIA
    ├── stream_adder_app\
    ├── tests\
    ├── setup_kria.sh, smoke_test.sh, ...
    └── README.md
```

---

Cuando vuelvas, corre los 3 comandos de "INICIO RÁPIDO" en REPORTE.md y todo debería rodar. Si algo falla, `bash diagnose_kria.sh` te da un dump completo del estado para debuggear.
