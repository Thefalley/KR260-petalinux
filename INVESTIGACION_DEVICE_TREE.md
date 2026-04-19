# Investigación: Device Tree, DMA y reserved-memory en Kria KR260

Research para el usuario. Cubre todos los aspectos relevantes para extender el stream_adder a transferencias DMA reales.

---

## 1. ¿Qué es un Device Tree Overlay (DTBO)?

Un DTBO es un "parche" que se aplica sobre el árbol de dispositivos del kernel **en runtime**. Permite añadir/modificar nodos sin reiniciar.

### Formato típico

```dts
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target = <&amba>;            // punto donde injectar
        __overlay__ {                // contenido a injectar
            my_device: my_device@a0010000 {
                compatible = "generic-uio";
                reg = <0x0 0xa0010000 0x0 0x1000>;
            };
        };
    };
};
```

### Compilación
```bash
dtc -@ -O dtb -o my.dtbo my.dtso
```
- `-@` activa referencias por nombre (labels) — imprescindible para overlays.

### Carga en runtime
- **Mediante xmutil** (Kria): `sudo xmutil loadapp <app_name>`
- **Mediante fpgautil**: `sudo fpgautil -b bit.bin -o overlay.dtbo`
- **Vía configfs**:
  ```bash
  sudo mkdir /sys/kernel/config/device-tree/overlays/my
  sudo cat my.dtbo > /sys/kernel/config/device-tree/overlays/my/dtbo
  ```

---

## 2. Reserved Memory — conceptos clave

### ¿Por qué se necesita?

Cuando un periférico DMA (como el AXI DMA) lee/escribe memoria RAM, necesita:
1. **Direcciones físicas contiguas** (o scatter-gather, que complica)
2. **Memoria que el kernel no reasigne** durante la transferencia
3. **Coherencia de caché** (que las writes del CPU se vean desde el DMA y viceversa)

### Tres formas de reservarla

**A) `reserved-memory` con `no-map`** — memoria excluida del mapa del kernel:
```dts
reserved-memory {
    #address-cells = <2>;
    #size-cells = <2>;
    ranges;
    
    dma_buffer: dma_buffer@70000000 {
        compatible = "shared-dma-pool";
        no-map;
        reg = <0x0 0x70000000 0x0 0x10000000>;  // 256MB a 0x70000000
    };
};
```
Uso: el dispositivo la referencia vía `memory-region = <&dma_buffer>;` y el driver usa `dma_alloc_coherent()`.

**B) `reserved-memory` con `reusable` (CMA)** — pool dinámico:
```dts
reserved-memory {
    cma_pool: cma_pool {
        compatible = "shared-dma-pool";
        reusable;                    // usable como RAM cuando no hay DMA
        size = <0x0 0x10000000>;     // 256MB
        linux,cma-default;           // usar como CMA por defecto
    };
};
```

**C) Kernel cmdline** — lo más fácil en Kria Ubuntu:
```
cma=512M
```
Añade 512MB a CMA global. El driver xlnx,axi-dma lo usa automáticamente.

### ⚠️ LIMITACIÓN IMPORTANTE

**`reserved-memory` NO SE PUEDE AÑADIR VIA OVERLAY RUNTIME.**

El kernel consume estos nodos muy pronto en el boot, antes de montar el rootfs y poder aplicar overlays. Si necesitas una reserved-memory nueva:
- Modificar el DTB base (en la imagen genérica de Kria, es `/boot/firmware/system-top.dtb` o similar)
- O añadir `cma=` al kernel cmdline en u-boot
- O compilar un kernel+dtb custom con la reserva

### ¿Qué tenemos en el overlay actual?

Nuestro `stream_adder.dtbo` NO reserva memoria — confía en CMA global. El driver `xlnx,axi-dma` llama a `dma_alloc_coherent()` que internamente saca páginas de CMA. Suele funcionar para transferencias pequeñas/medianas.

Comprobar CMA disponible en la placa:
```bash
cat /proc/meminfo | grep -i cma
# CmaTotal:   ...
# CmaFree:    ...
grep cma /proc/cmdline
```

---

## 3. Asociar memoria a un dispositivo (cuando existe la reserva)

Si tenemos `dma_buffer` reservada en DT base:

```dts
// En el overlay:
axi_dma_0: dma@a0000000 {
    compatible = "xlnx,axi-dma-1.00.a";
    reg = <0x0 0xa0000000 0x0 0x10000>;
    memory-region = <&dma_buffer>;       // <-- asocia
    // ...
};
```

El driver axi-dma hará `dma_declare_coherent_memory()` sobre esa región, y `dma_alloc_coherent()` sacará de ahí primero.

---

## 4. dma-ranges — el "mapa de direcciones físicas" del bus

El nodo padre AXI debe tener `dma-ranges` para decir qué rangos de memoria son visibles desde el periférico DMA.

Kria ZynqMP tiene en el DT base:
```dts
amba_pl: amba-pl@0 {
    #address-cells = <2>;
    #size-cells = <2>;
    dma-ranges = <0x0 0x0 0x0 0x0 0x1 0x0>;    // primeros 4GB accesibles
    // ...
};
```

Esto significa que los masters AXI en la PL pueden alcanzar la DDR baja (0x0 a 0x1_0000_0000 = 4GB).

### ¿Por qué nuestro DMA no ve DDR_HIGH?

En el build vimos este warning de Vivado:
```
WARNING: Cannot assign slave segment '/zynq_ps/SAXIGP2/HP0_DDR_HIGH' into
address space '/axi_dma_0/Data_MM2S' because no valid addressing path exists.
```

**Causa:** nuestro DMA usa `c_m_axi_mm2s_addr_width = 32` (4GB), y HP0_DDR_HIGH empieza en 0x8_0000_0000 (32GB). El DMA no puede direccionar ahí.

**Solución (si necesitas >4GB DMA):**
```tcl
# En el Tcl de Vivado:
set_property CONFIG.c_addr_width {40} $dma
set_property CONFIG.c_m_axi_mm2s_data_width {64} $dma  # opcional, más throughput
```

Pero para nuestro caso (transferencias pequeñas en los primeros 2GB), 32-bit basta.

---

## 5. UIO driver — acceso directo desde userspace

El overlay actual marca `stream_adder` como `compatible = "generic-uio"`. Esto hace que Linux:
1. Exponga `/dev/uio0` (o uio1, uio2...)
2. Permita mmap de la región `reg` de ese nodo
3. Reenvíe interrupts a userspace via `read()` en /dev/uioN

Ventajas:
- Sin kernel module — test rápido
- Control total desde Python/C

Inconvenientes:
- Sin DMA coherente automático
- Sin scatter-gather

Para el stream_adder (solo AXI-Lite, sin DMA): UIO es perfecto.

Para el AXI DMA: mejor usar el driver kernel `xlnx,axi-dma-1.00.a` que gestiona IRQs y descriptors.

---

## 6. Ejemplo completo — overlay con reserved-memory (teórico)

Si tuvieras control sobre el DT base:

```dts
// DT base (modificar system-top.dts):
/ {
    reserved-memory {
        #address-cells = <2>;
        #size-cells = <2>;
        ranges;
        
        dma_buffer_reserved: dma_buffer@70000000 {
            compatible = "shared-dma-pool";
            reusable;
            size = <0x0 0x10000000>;     // 256MB
            alignment = <0x0 0x1000>;
        };
    };
};
```

Y en el overlay:
```dts
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target = <&amba>;
        __overlay__ {
            axi_dma_0: dma@a0000000 {
                compatible = "xlnx,axi-dma-1.00.a";
                reg = <0x0 0xa0000000 0x0 0x10000>;
                memory-region = <&dma_buffer_reserved>;
                // clocks, interrupts...
            };
        };
    };
};
```

---

## 7. Pruebas concretas a hacer en la placa

### Nivel 1 — Ya funciona (sin overlay)
- `cat /proc/meminfo | grep Cma` → verifica CMA disponible
- `cat /proc/cmdline` → ver si hay `cma=...` en boot

### Nivel 2 — Tras cargar overlay
- `cat /sys/class/fpga_manager/fpga0/state` → operating
- `dmesg | tail -30` → ver logs del driver DMA y UIO
- `ls /dev/uio*` → listar UIOs (debe aparecer stream_adder)
- `cat /sys/class/uio/uio0/name` → comprobar nombre
- `cat /sys/class/uio/uio0/maps/map0/size` → tamaño mapeado

### Nivel 3 — Hello DMA (requiere driver xilinx_dmaengine)
```bash
sudo modprobe xilinx_dmaengine
sudo modprobe xilinx_axidmatest channel=dma0chan0 test_buf_size=4096 iterations=1
dmesg | tail -20
```

### Nivel 4 — Transferencia DMA completa desde userspace
Requiere:
- Módulo `udmabuf` cargado (ver https://github.com/ikwzm/udmabuf)
- O driver `dma-proxy` custom (ver https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842041/Linux+DMA+From+User+Space)

Flujo:
1. Crear buffer contiguo via udmabuf: `echo 1048576 > /sys/class/udmabuf/udmabuf0/size`
2. Obtener dirección física: `cat /sys/class/udmabuf/udmabuf0/phys_addr`
3. mmap `/dev/udmabuf0` para acceso CPU
4. Escribir datos de entrada
5. Programar MM2S: source = phys_addr, length = N bytes
6. Programar S2MM: dest = phys_addr + offset, length = N bytes
7. Arrancar con RS=1 en DMACRs
8. Esperar DMASR.IDLE o IRQ
9. Leer resultado via mmap del buffer

---

## 8. Referencias oficiales

- **PG021** - AXI DMA Product Guide (Xilinx): describe los registros, modo direct/scatter-gather
- **UG1144** - PetaLinux Tools Documentation Reference Guide
- **UG1085** - Zynq UltraScale+ MPSoC Technical Reference Manual  
- Kria wiki: https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/1900479362/Kria+Starter+Kit+Welcome+Page
- PYNQ for Kria: https://github.com/Xilinx/PYNQ
- u-dma-buf (contiguous memory from userspace): https://github.com/ikwzm/udmabuf

---

## 9. Modificaciones sugeridas al stream_adder para producción

Si acabas queriendo escalar el stream_adder (no como demo):

### 9.1 Aumentar addr_width del DMA
```tcl
CONFIG.c_addr_width {40}    # ver toda la DDR (64GB posibles)
```

### 9.2 Data width a 64 bits
```tcl
CONFIG.c_m_axi_mm2s_data_width {64}
CONFIG.c_m_axis_mm2s_tdata_width {64}
```
Y actualizar stream_adder.vhd para `DATA_WIDTH = 64`.

### 9.3 Activar scatter-gather
```tcl
CONFIG.c_include_sg {1}
CONFIG.c_sg_length_width {26}    # hasta 64MB por descriptor
```

Permite listas de descriptores — útil para transferencias grandes sin bloquear CPU.

### 9.4 Reservar memoria en DT base
Modificar `/boot/firmware/system-top.dtb` para añadir `dma_buffer_reserved` 256MB-1GB.

### 9.5 Escribir un driver kernel custom (avanzado)
- Registrar platform_driver con `compatible = "xlnx,stream-adder-1.0"`
- Usar dmaengine para gestionar transferencias
- Exponer /dev/stream_adder char device

---

## 10. Comandos para copiar-pegar (cuando Kria responda)

```bash
# === Diagnóstico base ===
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150

# === En la Kria ===
# CMA disponible:
grep -i cma /proc/meminfo /proc/cmdline

# DT base del sistema:
ls /sys/firmware/devicetree/base/
dtc -I fs /sys/firmware/devicetree/base 2>/dev/null | grep -A5 reserved-memory | head -20

# Apps Kria:
sudo xmutil listapps

# Modulos xilinx cargados:
lsmod | grep -iE "xilinx|xlnx|axi|dma"
```
