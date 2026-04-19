#!/bin/bash
set -e
source /home/jce03/petalinux-instalado/settings.sh >/dev/null 2>&1 || true

PKG=/mnt/e/petalinux-instalado/deploy_kr260/stream_adder_app
HWDIR=/home/jce03/petalinux-proyectos/P_01/project-spec/hw-description

mkdir -p "$PKG"
cd "$PKG"

echo "=== 1. BIF para bootgen (solo bitstream) ==="
cat > stream_adder.bif << EOF
all:
{
    [destination_device = pl] ${HWDIR}/design_1_wrapper.bit
}
EOF
cat stream_adder.bif

echo ""
echo "=== 2. Generando stream_adder.bit.bin con bootgen ==="
bootgen -arch zynqmp -image stream_adder.bif -w -o stream_adder.bit.bin -process_bitstream bin 2>&1 | tail -10
# bootgen may output different name; find and move
for f in "${HWDIR}"/design_1_wrapper.bit.bin "${PKG}/design_1_wrapper.bit.bin"; do
    [ -f "$f" ] && mv "$f" "${PKG}/stream_adder.bit.bin"
done
ls -lh "${PKG}/stream_adder.bit.bin" 2>&1

echo ""
echo "=== 3. Generando stream_adder.dtbo ==="
cat > stream_adder.dtso << 'DTSEOF'
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target = <&amba>;
        __overlay__ {
            #address-cells = <2>;
            #size-cells = <2>;

            afi0: afi0 {
                compatible = "xlnx,afi-fpga";
                config-afi = <0 0>, <1 0>, <2 0>, <3 0>, <4 0>, <5 0>,
                             <6 0>, <7 0>, <8 0>, <9 0>, <10 0>, <11 0>,
                             <12 0>, <13 0>, <14 0>, <15 0>;
            };

            clocking0: clocking0 {
                #clock-cells = <0>;
                assigned-clock-rates = <100000000>;
                assigned-clocks = <&zynqmp_clk 71>;
                clock-output-names = "fabric_clk";
                clocks = <&zynqmp_clk 71>;
                compatible = "xlnx,fclk";
            };

            axi_dma_0: dma@a0000000 {
                #dma-cells = <1>;
                clock-names = "m_axi_mm2s_aclk", "m_axi_s2mm_aclk", "s_axi_lite_aclk";
                clocks = <&zynqmp_clk 71>, <&zynqmp_clk 71>, <&zynqmp_clk 71>;
                compatible = "xlnx,axi-dma-7.1", "xlnx,axi-dma-1.00.a";
                interrupt-names = "mm2s_introut", "s2mm_introut";
                interrupt-parent = <&gic>;
                interrupts = <0 89 4>, <0 90 4>;
                reg = <0x0 0xa0000000 0x0 0x10000>;
                xlnx,addrwidth = <0x20>;
                xlnx,sg-length-width = <0x10>;
                dma-channel@a0000000 {
                    compatible = "xlnx,axi-dma-mm2s-channel";
                    dma-channels = <0x1>;
                    interrupts = <0 89 4>;
                    xlnx,datawidth = <0x20>;
                    xlnx,device-id = <0x0>;
                    xlnx,include-dre;
                };
                dma-channel@a0000030 {
                    compatible = "xlnx,axi-dma-s2mm-channel";
                    dma-channels = <0x1>;
                    interrupts = <0 90 4>;
                    xlnx,datawidth = <0x20>;
                    xlnx,device-id = <0x0>;
                    xlnx,include-dre;
                };
            };

            stream_adder_0: stream_adder@a0010000 {
                clock-names = "clk";
                clocks = <&zynqmp_clk 71>;
                compatible = "generic-uio";
                reg = <0x0 0xa0010000 0x0 0x1000>;
            };
        };
    };
};
DTSEOF

dtc -@ -O dtb -o stream_adder.dtbo stream_adder.dtso 2>&1
ls -lh stream_adder.dtbo

echo ""
echo "=== 4. shell.json ==="
cat > shell.json << 'JSONEOF'
{
    "shell_type" : "XRT_FLAT",
    "num_slots" : "1"
}
JSONEOF
cat shell.json

echo ""
echo "=== Contenido paquete ==="
ls -lh "${PKG}/"
