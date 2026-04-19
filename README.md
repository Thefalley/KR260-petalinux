# PetaLinux + Kria KR260 — Índice de documentación

Este directorio contiene todo el trabajo: proyectos Vivado, build de PetaLinux, accelerator package, tests y documentación.

## 📚 Documentos por utilidad

| Quiero… | Leer esto |
|---|---|
| **Saber exactamente qué comandos ejecutar cuando vuelva** | [`CHEAT_SHEET.md`](CHEAT_SHEET.md) |
| **Entender qué se hizo mientras comía (narrativa)** | [`REPORTE_NARRATIVO.md`](REPORTE_NARRATIVO.md) |
| **Referencia técnica completa** | [`REPORTE.md`](REPORTE.md) |
| **Profundizar en device tree, DMA, reserved-memory** | [`INVESTIGACION_DEVICE_TREE.md`](INVESTIGACION_DEVICE_TREE.md) |
| **Instrucciones de ayer para setup noche** | [`INSTRUCCIONES_MAÑANA.md`](INSTRUCCIONES_MAÑANA.md) *(obsoletas tras pipeline noche)* |

## 📁 Directorios

| Carpeta | Qué contiene |
|---|---|
| `P_3_stream_adder\` | Proyecto ORIGINAL ZedBoard — **no tocar**, es la referencia |
| `P_3_stream_adder_kr260\` | **Migración a KR260** — VHDL + Tcl + XSA + bitstream |
| `P_02_simple_axi_reg\` | Segundo proyecto más simple (4 registros AXI-Lite) — útil para debug |
| `deploy_kr260\` | ★ **KIT LISTO PARA LA KRIA** — accelerator, tests, scripts |
| `build_accel_package.sh` | Script que genera el .bit.bin + .dtbo + shell.json |
| `pipeline_nocturno.sh` | Pipeline que corrió la noche anterior (ya ejecutado) |
| `read.txt` | Vacío (probablemente placeholder) |

## 🚀 Arrancar ya

```bash
# En una terminal WSL/bash:
cat CHEAT_SHEET.md | head -30
```

O directo:

```powershell
# 1. Copiar kit a la Kria (desde Windows):
scp -J rubme@100.127.187.82 "E:\petalinux-instalado\deploy_kr260\kria_kit.tar.gz" ubuntu@192.168.1.150:/tmp/

# 2. SSH:
ssh -J rubme@100.127.187.82 ubuntu@192.168.1.150
```

```bash
# 3. En la Kria:
cd /tmp
tar xzf kria_kit.tar.gz            # (ya extraído si lo hice antes)
bash setup_kria.sh                 # pide password UNA VEZ
bash smoke_test.sh                 # corre todos los tests
```

---

## 🔄 Flujo de trabajo completo

```
        (1) VHDL ZedBoard
       P_3_stream_adder/src/
                │
                ▼
        (2) Migracion a KR260
     P_3_stream_adder_kr260/
                │
                ▼
           Vivado batch
      (build_kr260.tcl, ~45 min)
                │
                ▼
        design_1_wrapper.xsa
                │
                ▼
        (3) PetaLinux
     petalinux-proyectos/P_01/
     petalinux-config + build
          (~90 min)
                │
                ▼
           system.bit
                │
                ▼
        (4) Accelerator package
     build_accel_package.sh
                │
                ▼
     deploy_kr260/stream_adder_app/
     (bit.bin + dtbo + shell.json)
                │
                ▼
        (5) Carga en Kria
       xmutil loadapp stream_adder
                │
                ▼
           🎯 TESTS
        (C + Python, smoke test)
```

---

## Estado actual

- ✅ Pasos 1-4 **completados**
- ⏸ Paso 5 **pendiente tú** (requiere tu password una vez)
- Tarball ya subido + extraído en `/tmp/` de la Kria
- Tests ya compilados en `/tmp/tests/test_*`

---

## Generado por

Claude Code + tú, 18-19 abril 2026.  
Todo el código/scripts son tuyos, úsalos como quieras.
