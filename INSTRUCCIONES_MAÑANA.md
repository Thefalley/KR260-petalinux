# Instrucciones para mañana — Kria KR260 stream_adder

## 1. Comprobar que todo se compiló OK

Abre PowerShell y ejecuta:

```powershell
wsl -d Ubuntu-22.04 -- bash -c "ls -la /home/jce03/PIPELINE_DONE.flag && tail -30 /home/jce03/pipeline_nocturno.log"
```

- Si `PIPELINE_DONE.flag` existe → todo OK, sigue al paso 2
- Si no → mira el log: `wsl -- bash -c "tail -100 /home/jce03/pipeline_nocturno.log"`

## 2. Archivos listos para SD

Están en: `E:\petalinux-instalado\deploy_kr260\`

- `BOOT.BIN` — first-stage boot + bitstream + u-boot
- `image.ub` — kernel + device tree + rootfs
- `boot.scr` — script U-Boot
- `rootfs.tar.gz` — rootfs extraíble

## 3. Preparar la SD card (desde Windows)

**Método recomendado — con balenaEtcher:**
Si hay un `.wic.xz` en `deploy_kr260\`, usa ese directamente con Etcher.

**Método manual:**
1. Formatea una partición FAT32 de 512 MB, label `BOOT`
2. Copia a `BOOT`: `BOOT.BIN`, `image.ub`, `boot.scr`
3. Crea segunda partición ext4 (o déjala como FAT32 para rootfs inicial)
4. Arranca la KR260 desde SD (DIP switch)

## 4. Conectar por SSH

Al arrancar, la KR260 obtendrá IP por DHCP. Login por defecto:
- Usuario: `root` o `petalinux`
- Password: `root` / `petalinux` (primera vez te pide cambiar)

```bash
ssh petalinux@<ip_kr260>
```

## 5. Probar stream_adder

Una vez dentro de la KR260:

```bash
# Ver que el DMA y el stream_adder están en el árbol de dispositivos
ls /sys/class/uio/
dmesg | grep -iE "dma|stream"

# Ver que el bitstream se cargó
cat /sys/class/fpga_manager/fpga0/state
# Debería decir: operating
```

### App hello-kr260 (está en /usr/bin)
```bash
hello-kr260
```

### Stream adder (prueba básica con DMA)
Las direcciones son:
- AXI DMA: `0xA000_0000`
- stream_adder (reg control): `0xA001_0000`

Para test rápido de AXI-Lite a stream_adder:
```bash
# Escribir add_value=5 al registro 0
devmem 0xA0010000 32 0x5
# Leer de vuelta
devmem 0xA0010000 32
```

## 6. Si algo falla

**Build falló:**
```bash
wsl -- bash -c "cd /home/jce03/petalinux-proyectos/P_01 && source /home/jce03/petalinux-instalado/settings.sh && petalinux-build 2>&1 | tail -50"
```

**Recompilar desde cero:**
```bash
wsl -- bash -c "cd /home/jce03/petalinux-proyectos/P_01 && source /home/jce03/petalinux-instalado/settings.sh && petalinux-build -x mrproper && petalinux-build"
```

**Ver XSA importado:**
```bash
wsl -- bash -c "ls -la /home/jce03/petalinux-proyectos/P_01/project-spec/hw-description/"
```

## Resumen rutas

| Qué | Dónde |
|---|---|
| Vivado project (KR260) | `E:\petalinux-instalado\P_3_stream_adder_kr260\` |
| XSA | `E:\petalinux-instalado\P_3_stream_adder_kr260\design_1_wrapper.xsa` |
| PetaLinux proyecto | WSL: `/home/jce03/petalinux-proyectos/P_01/` |
| Imágenes para SD | `E:\petalinux-instalado\deploy_kr260\` |
| Log pipeline nocturna | WSL: `/home/jce03/pipeline_nocturno.log` |
