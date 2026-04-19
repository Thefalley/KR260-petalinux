#!/bin/bash
# Generar accelerator package de P_02 (simple_axi_reg)
set -e
source /home/jce03/petalinux-instalado/settings.sh >/dev/null 2>&1 || true

PKG=/mnt/e/petalinux-instalado/deploy_kr260/simple_axi_app
BIT=/mnt/e/petalinux-instalado/P_02_simple_axi_reg/vivado_project/kr260_simple_axi.runs/impl_1/design_1_wrapper.bit

mkdir -p "$PKG"
cd "$PKG"

echo "=== BIF para bootgen ==="
cat > simple_axi.bif << EOF
all:
{
    [destination_device = pl] ${BIT}
}
EOF

echo "=== bootgen -> simple_axi.bit.bin ==="
bootgen -arch zynqmp -image simple_axi.bif -w -o simple_axi.bit.bin -process_bitstream bin 2>&1 | tail -5
for f in "$(dirname ${BIT})/design_1_wrapper.bit.bin" "${PKG}/design_1_wrapper.bit.bin"; do
    [ -f "$f" ] && mv "$f" "${PKG}/simple_axi.bit.bin"
done
ls -lh "${PKG}/simple_axi.bit.bin"

echo "=== simple_axi.dtbo ==="
cat > simple_axi.dtso << 'DTSEOF'
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target = <&amba>;
        __overlay__ {
            #address-cells = <2>;
            #size-cells = <2>;

            clocking0: clocking0 {
                #clock-cells = <0>;
                assigned-clock-rates = <100000000>;
                assigned-clocks = <&zynqmp_clk 71>;
                clock-output-names = "fabric_clk";
                clocks = <&zynqmp_clk 71>;
                compatible = "xlnx,fclk";
            };

            simple_axi_reg_0: simple_axi_reg@a0000000 {
                clock-names = "s_axi_aclk";
                clocks = <&zynqmp_clk 71>;
                compatible = "generic-uio";
                reg = <0x0 0xa0000000 0x0 0x1000>;
            };
        };
    };
};
DTSEOF

dtc -@ -O dtb -o simple_axi.dtbo simple_axi.dtso 2>&1
ls -lh simple_axi.dtbo

echo "=== shell.json ==="
cat > shell.json << 'JSONEOF'
{
    "shell_type" : "XRT_FLAT",
    "num_slots" : "1"
}
JSONEOF

echo "=== Contenido ==="
ls -lh "${PKG}/"
