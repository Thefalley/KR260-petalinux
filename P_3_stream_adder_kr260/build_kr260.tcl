###############################################################
# build_kr260.tcl
# Vivado 2025.2 batch build: stream_adder -> KR260 (K26 SOM)
# Genera bitstream y exporta XSA para PetaLinux
###############################################################

set project_name "kr260_stream_adder"
set project_dir "./vivado_project"
set part "xck26-sfvc784-2LV-c"

# Intentar varios nombres de board_part (segun version)
set board_part_candidates {
    "xilinx.com:kr260_som:part0:1.1"
    "xilinx.com:kr260_som:part0:1.0"
    "xilinx.com:kr260_som_carrier:part0:1.0"
}

puts "=== Limpiando proyecto anterior ==="
if {[file exists "$project_dir/$project_name.xpr"]} {
    file delete -force $project_dir
}

puts "=== Creando proyecto Vivado ==="
create_project $project_name $project_dir -part $part -force

# Intentar board_part
set board_set 0
foreach bp $board_part_candidates {
    if {![catch {set_property board_part $bp [current_project]} err]} {
        puts "INFO: board_part = $bp"
        set board_set 1
        break
    }
}
if {$board_set == 0} {
    puts "WARN: No KR260 board files found - usando solo part $part"
}

puts "=== Anadiendo fuentes VHDL ==="
add_files -norecurse [list \
    "./src/axi_lite_cfg.vhd" \
    "./src/HsSkidBuf_dest.vhd" \
    "./src/stream_adder.vhd" \
]
set_property file_type {VHDL} [get_files -of [current_fileset] *.vhd]
update_compile_order -fileset sources_1

puts "=== Creando block design ==="
create_bd_design "design_1"

# ---- Zynq UltraScale+ PS ----
set ps [create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ps]

# Aplicar preset de board si board_part cargado, sino config minima
if {$board_set == 1} {
    if {[catch {apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1"} $ps} err]} {
        puts "WARN: apply_board_preset fallo: $err - usando defaults"
    } else {
        puts "INFO: Board preset aplicado"
    }
}

# Forzar configuracion minima requerida
# GP0 = HPM0_FPD (activado), GP1 = HPM1_FPD (desactivado), GP2 = HPM0_LPD (desactivado)
set_property -dict [list \
    CONFIG.PSU__USE__M_AXI_GP0 {1} \
    CONFIG.PSU__USE__M_AXI_GP1 {0} \
    CONFIG.PSU__USE__M_AXI_GP2 {0} \
    CONFIG.PSU__USE__S_AXI_GP0 {0} \
    CONFIG.PSU__USE__S_AXI_GP1 {0} \
    CONFIG.PSU__USE__S_AXI_GP2 {1} \
    CONFIG.PSU__USE__S_AXI_GP3 {0} \
    CONFIG.PSU__USE__S_AXI_GP4 {0} \
    CONFIG.PSU__USE__S_AXI_GP5 {0} \
    CONFIG.PSU__FPGA_PL0_ENABLE {1} \
    CONFIG.PSU__FPGA_PL1_ENABLE {0} \
    CONFIG.PSU__FPGA_PL2_ENABLE {0} \
    CONFIG.PSU__FPGA_PL3_ENABLE {0} \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {100} \
    CONFIG.PSU__USE__IRQ0 {1} \
] $ps

# ---- AXI DMA (para feed stream_adder) ----
set dma [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma axi_dma_0]
set_property -dict [list \
    CONFIG.c_include_sg {0} \
    CONFIG.c_sg_length_width {16} \
    CONFIG.c_include_mm2s_dre {1} \
    CONFIG.c_include_s2mm_dre {1} \
    CONFIG.c_m_axi_mm2s_data_width {32} \
    CONFIG.c_m_axis_mm2s_tdata_width {32} \
    CONFIG.c_mm2s_burst_size {16} \
    CONFIG.c_s_axis_s2mm_tdata_width {32} \
    CONFIG.c_m_axi_s2mm_data_width {32} \
    CONFIG.c_s2mm_burst_size {16} \
] $dma

# ---- Stream adder como modulo RTL ----
set adder [create_bd_cell -type module -reference stream_adder stream_adder_0]

# ---- SmartConnect AXI-Lite (PS -> DMA.s_axi_lite + adder.S_AXI) ----
set sc_lite [create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect sc_lite]
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {2}] $sc_lite

# ---- SmartConnect AXI-Full (DMA masters -> PS S_AXI_HP0) ----
set sc_mem [create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect sc_mem]
set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {1}] $sc_mem

# ---- Processor System Reset ----
set rst [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_0]

# ---- Concat para IRQs del DMA ----
set xlcat [create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat xlconcat_0]
set_property CONFIG.NUM_PORTS {2} $xlcat

puts "=== Conectando clocks ==="
set clk [get_bd_pins $ps/pl_clk0]
connect_bd_net $clk [get_bd_pins $rst/slowest_sync_clk]
connect_bd_net $clk [get_bd_pins $dma/s_axi_lite_aclk]
connect_bd_net $clk [get_bd_pins $dma/m_axi_mm2s_aclk]
connect_bd_net $clk [get_bd_pins $dma/m_axi_s2mm_aclk]
connect_bd_net $clk [get_bd_pins $adder/clk]
connect_bd_net $clk [get_bd_pins $sc_lite/aclk]
connect_bd_net $clk [get_bd_pins $sc_mem/aclk]
connect_bd_net $clk [get_bd_pins $ps/maxihpm0_fpd_aclk]
connect_bd_net $clk [get_bd_pins $ps/saxihp0_fpd_aclk]

puts "=== Conectando resets ==="
connect_bd_net [get_bd_pins $ps/pl_resetn0] [get_bd_pins $rst/ext_reset_in]
set rstn [get_bd_pins $rst/peripheral_aresetn]
connect_bd_net $rstn [get_bd_pins $adder/resetn]
connect_bd_net $rstn [get_bd_pins $dma/axi_resetn]
connect_bd_net $rstn [get_bd_pins $sc_lite/aresetn]
connect_bd_net $rstn [get_bd_pins $sc_mem/aresetn]

puts "=== AXI-Lite: PS HPM0 -> SmartConnect -> DMA + Adder ==="
connect_bd_intf_net [get_bd_intf_pins $ps/M_AXI_HPM0_FPD] [get_bd_intf_pins $sc_lite/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $sc_lite/M00_AXI] [get_bd_intf_pins $dma/S_AXI_LITE]
connect_bd_intf_net [get_bd_intf_pins $sc_lite/M01_AXI] [get_bd_intf_pins $adder/S_AXI]

puts "=== AXI-Stream: DMA <-> stream_adder ==="
connect_bd_intf_net [get_bd_intf_pins $dma/M_AXIS_MM2S] [get_bd_intf_pins $adder/s_axis]
connect_bd_intf_net [get_bd_intf_pins $adder/m_axis] [get_bd_intf_pins $dma/S_AXIS_S2MM]

puts "=== AXI-Full: DMA masters -> PS HP0 ==="
connect_bd_intf_net [get_bd_intf_pins $dma/M_AXI_MM2S] [get_bd_intf_pins $sc_mem/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $dma/M_AXI_S2MM] [get_bd_intf_pins $sc_mem/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins $sc_mem/M00_AXI] [get_bd_intf_pins $ps/S_AXI_HP0_FPD]

puts "=== Interrupts DMA -> PS ==="
connect_bd_net [get_bd_pins $dma/mm2s_introut] [get_bd_pins $xlcat/In0]
connect_bd_net [get_bd_pins $dma/s2mm_introut] [get_bd_pins $xlcat/In1]
connect_bd_net [get_bd_pins $xlcat/dout] [get_bd_pins $ps/pl_ps_irq0]

puts "=== Address map ==="
assign_bd_address

puts "=== Validando ==="
validate_bd_design

puts "=== Guardando block design ==="
save_bd_design

puts "=== Creando HDL wrapper ==="
set bd_file [get_files design_1.bd]
make_wrapper -files $bd_file -top -import
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1

puts "=== Synthesis ==="
launch_runs synth_1 -jobs 4
wait_on_run synth_1

if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    error "ERROR: synth_1 fallo"
}

puts "=== Implementation + bitstream ==="
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    error "ERROR: impl_1 fallo"
}

puts "=== Exportando XSA ==="
open_run impl_1
write_hw_platform -fixed -include_bit -force -file "./design_1_wrapper.xsa"

puts ""
puts "==========================================================="
puts "=== BUILD COMPLETADO CON EXITO ==="
puts "XSA: [file normalize ./design_1_wrapper.xsa]"
puts "==========================================================="
