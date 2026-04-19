###############################################################
# build_p02.tcl
# Vivado 2025.2 - simple_axi_reg en KR260 (diseño minimo)
# Solo PS + AXI-Lite al simple_axi_reg, sin DMA ni streams
###############################################################

set project_name "kr260_simple_axi"
set project_dir "./vivado_project"
set part "xck26-sfvc784-2LV-c"

set board_part_candidates {
    "xilinx.com:kr260_som:part0:1.1"
    "xilinx.com:kr260_som:part0:1.0"
}

puts "=== Limpiando ==="
if {[file exists "$project_dir/$project_name.xpr"]} {
    file delete -force $project_dir
}

puts "=== Creando proyecto ==="
create_project $project_name $project_dir -part $part -force

set board_set 0
foreach bp $board_part_candidates {
    if {![catch {set_property board_part $bp [current_project]} err]} {
        set board_set 1
        break
    }
}

puts "=== Anadiendo fuente VHDL ==="
add_files -norecurse [list "./src/simple_axi_reg.vhd"]
set_property file_type {VHDL} [get_files *.vhd]
update_compile_order -fileset sources_1

puts "=== Block design ==="
create_bd_design "design_1"

set ps [create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ps]
if {$board_set == 1} {
    catch {apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1"} $ps}
}

# Config minima: solo HPM0 FPD + PL_CLK0 + resetn
set_property -dict [list \
    CONFIG.PSU__USE__M_AXI_GP0 {1} \
    CONFIG.PSU__USE__M_AXI_GP1 {0} \
    CONFIG.PSU__USE__M_AXI_GP2 {0} \
    CONFIG.PSU__USE__S_AXI_GP0 {0} \
    CONFIG.PSU__USE__S_AXI_GP1 {0} \
    CONFIG.PSU__USE__S_AXI_GP2 {0} \
    CONFIG.PSU__FPGA_PL0_ENABLE {1} \
    CONFIG.PSU__FPGA_PL1_ENABLE {0} \
    CONFIG.PSU__FPGA_PL2_ENABLE {0} \
    CONFIG.PSU__FPGA_PL3_ENABLE {0} \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {100} \
    CONFIG.PSU__USE__IRQ0 {0} \
] $ps

# simple_axi_reg como modulo RTL
set reg [create_bd_cell -type module -reference simple_axi_reg simple_axi_reg_0]

# Reset
set rst [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_0]

# SmartConnect para convertir AXI4 (PS) -> AXI4-Lite (simple_axi_reg)
set sc [create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect sc_axi]
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] $sc

puts "=== Conexiones ==="
set clk [get_bd_pins $ps/pl_clk0]
connect_bd_net $clk [get_bd_pins $rst/slowest_sync_clk]
connect_bd_net $clk [get_bd_pins $reg/S_AXI_ACLK]
connect_bd_net $clk [get_bd_pins $ps/maxihpm0_fpd_aclk]
connect_bd_net $clk [get_bd_pins $sc/aclk]

connect_bd_net [get_bd_pins $ps/pl_resetn0] [get_bd_pins $rst/ext_reset_in]
connect_bd_net [get_bd_pins $rst/peripheral_aresetn] [get_bd_pins $reg/S_AXI_ARESETN]
connect_bd_net [get_bd_pins $rst/peripheral_aresetn] [get_bd_pins $sc/aresetn]

# PS HPM0 (AXI4) -> SmartConnect -> simple_axi_reg (AXI4-Lite)
connect_bd_intf_net [get_bd_intf_pins $ps/M_AXI_HPM0_FPD] [get_bd_intf_pins $sc/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins $sc/M00_AXI] [get_bd_intf_pins $reg/S_AXI]

puts "=== Address map ==="
assign_bd_address

puts "=== Validando ==="
validate_bd_design

puts "=== HDL wrapper ==="
set bd_file [get_files design_1.bd]
make_wrapper -files $bd_file -top -import
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1

puts "=== Synth ==="
launch_runs synth_1 -jobs 4
wait_on_run synth_1

puts "=== Impl + bitstream ==="
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

puts "=== Export XSA ==="
open_run impl_1
write_hw_platform -fixed -include_bit -force -file "./p02_simple_axi.xsa"

puts "=========================================================="
puts "BUILD_P02_OK"
puts "XSA: [file normalize ./p02_simple_axi.xsa]"
puts "=========================================================="
