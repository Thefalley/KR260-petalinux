# 🎯 Cheat Sheet — Kria KR260 stream_adder

## Arrancar la primera demo (una vez vuelvas)

```powershell
# 1. SSH a la Kria
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150

# O si estás en desktop-6j3hg3r:
ssh kria
```

```bash
# 2. Dentro de la Kria - el tarball ya está extraído en /tmp/
cd /tmp
bash setup_kria.sh            # pide tu password UNA vez
bash smoke_test.sh            # corre todos los tests
```

---

## Si necesitas re-copiar el tarball

```powershell
# Desde Windows (mi pc)
scp -J rubme@100.127.187.82 "E:\petalinux-instalado\deploy_kr260\kria_kit.tar.gz" ubuntu@192.168.1.150:/tmp/
```

```bash
# En la Kria
cd /tmp && tar xzf kria_kit.tar.gz && ls
```

---

## Comandos sueltos útiles

### Ver estado
```bash
cat /sys/class/fpga_manager/fpga0/state    # "operating" = PL programado
sudo xmutil listapps                        # lista apps disponibles + Active
```

### Cargar stream_adder
```bash
sudo xmutil unloadapp 2>/dev/null           # descarga actual
sudo xmutil loadapp stream_adder
```

### Cargar app genérico de Kria (reset)
```bash
sudo xmutil unloadapp
sudo xmutil loadapp k26-starter-kits
```

### Leer registro del stream_adder (add_value)
```bash
sudo devmem 0xA0010000 32                   # leer
sudo devmem 0xA0010000 32 0x42              # escribir 0x42
sudo devmem 0xA0010000 32                   # leer (debe salir 0x42)
```

### Ver UIOs (debe aparecer uno nuevo tras cargar stream_adder)
```bash
ls /dev/uio*
for i in /sys/class/uio/uio*; do echo -n "$i: "; cat $i/name; done
```

### Ver device tree runtime
```bash
ls /sys/firmware/devicetree/base/
find /sys/firmware/devicetree/base -name "stream_adder*" -o -name "dma@*"
```

### Ver dmesg filtrado
```bash
sudo dmesg | grep -iE "fpga|dma|xlnx|stream" | tail -20
```

### Diagnóstico completo (si algo falla)
```bash
bash /tmp/diagnose_kria.sh
cat /tmp/kria_diag_*.txt                    # resultado
```

---

## Rebuilds

### Regenerar bitstream (Vivado en tu PC Windows)
```powershell
cd E:\petalinux-instalado\P_3_stream_adder_kr260
E:\vivado-instalado\2025.2.1\Vivado\bin\vivado.bat -mode batch -source build_kr260.tcl
```

### Regenerar accelerator package (en WSL)
```bash
wsl -d Ubuntu-22.04 -- bash /mnt/e/petalinux-instalado/build_accel_package.sh
```

### Regenerar tarball
```bash
cd E:/petalinux-instalado/deploy_kr260 && tar czf kria_kit.tar.gz stream_adder_app/ tests/ *.sh README.md
```

---

## Troubleshooting

### `FPGA state != operating` tras loadapp
- Verificar `ls /lib/firmware/xilinx/stream_adder/` → deben existir los 3 archivos
- Permisos: `sudo chmod 644 /lib/firmware/xilinx/stream_adder/*`
- Ver logs: `sudo dmesg | grep -iE 'fpga|dtbo' | tail -20`

### `test_stream_adder` lee todo 0xFFFFFFFF
- El bitstream no está cargado, el PS lee bus error y devuelve 0xFFFFFFFF
- → `sudo xmutil loadapp stream_adder` primero

### `xmutil: Permission denied`
- Falta NOPASSWD en sudoers. Correr `bash setup_kria.sh`

### Connection closed / timeout SSH
- Kria en switch aislado con conexión inestable
- Reintentar. Si persiste, ping desde desktop-6j3hg3r: `ping 192.168.1.150`

### `test_dma_full: /dev/udmabuf0 no existe`
- Cargar modulo: `bash /tmp/install_udmabuf.sh` (tarda ~3 min, compila contra headers kernel)

### Clock skew warning al extraer tar
- Kria reloj atrasado vs mi PC. Inofensivo. Si quieres, `sudo ntpdate pool.ntp.org`.

---

## Memoria y mapa de direcciones

| Region | Dir | Tamaño | Qué |
|---|---|---|---|
| DDR low | 0x00000000 | 2 GB | RAM acceso DMA |
| AXI DMA regs | 0xA0000000 | 64 KB | PG021 — MM2S/S2MM control |
| stream_adder regs | 0xA0010000 | 4 KB | Solo reg0 (add_value) activo |

Registros DMA clave (dentro de 0xA0000000):
- 0x00 MM2S_DMACR (control mm2s)
- 0x04 MM2S_DMASR (status mm2s)
- 0x18 MM2S_SA    (source addr)
- 0x28 MM2S_LENGTH (triggers transfer)
- 0x30 S2MM_DMACR
- 0x34 S2MM_DMASR  
- 0x48 S2MM_DA
- 0x58 S2MM_LENGTH

---

## Accesos SSH

| Desde | A | Comando |
|---|---|---|
| mi PC Windows | desktop-6j3hg3r | `ssh rubme@100.127.187.82` |
| mi PC Windows | Kria | `ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150` |
| desktop-6j3hg3r | Kria | `ssh kria` |

Claves SSH:
- `C:\Users\jce03\.ssh\id_ed25519` - mi clave privada en este PC
- Pubkey ya instalada en: rubme@desktop-6j3hg3r y ubuntu@kria
