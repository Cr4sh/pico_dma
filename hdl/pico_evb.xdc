###############################################################################
# Pinout and Related I/O Constraints
###############################################################################

# SYS reset (input) signal. The sys_reset_n signal is generated
# by the PCI Express interface (PERST#).
set_property PACKAGE_PIN A10 [get_ports sys_rst_n]
set_property IOSTANDARD LVCMOS33 [get_ports sys_rst_n]
set_property PULLDOWN true [get_ports sys_rst_n]

# SYS clock 100 MHz (input) signal. The sys_clk_p and sys_clk_n
# signals are the PCI Express reference clock. 
set_property PACKAGE_PIN B6 [get_ports sys_clk_p]

# PCIe x1 link
set_property PACKAGE_PIN G4 [get_ports pcie_rxp]
set_property PACKAGE_PIN G3 [get_ports pcie_rxn]
set_property PACKAGE_PIN B2 [get_ports pcie_txp]
set_property PACKAGE_PIN B1 [get_ports pcie_txn]

###############################################################################
# Timing Constraints
###############################################################################

create_clock -period 10.000 -name sys_clk [get_ports sys_clk_p]

###############################################################################
# Physical Constraints
###############################################################################

# Input reset is resynchronized within FPGA design as necessary
set_false_path -from [get_ports sys_rst_n]

###############################################################################
# SPI
###############################################################################

set_property PACKAGE_PIN K16 [get_ports {spi_io0_io}]
set_property PACKAGE_PIN L17 [get_ports {spi_io1_io}]
set_property PACKAGE_PIN J15 [get_ports {spi_io2_io}]
set_property PACKAGE_PIN J16 [get_ports {spi_io3_io}]
set_property PACKAGE_PIN L15 [get_ports {spi_ss_io}]
set_property IOSTANDARD LVCMOS33 [get_ports {spi_io0_io}]
set_property IOSTANDARD LVCMOS33 [get_ports {spi_io1_io}]
set_property IOSTANDARD LVCMOS33 [get_ports {spi_io2_io}]
set_property IOSTANDARD LVCMOS33 [get_ports {spi_io3_io}]
set_property IOSTANDARD LVCMOS33 [get_ports {spi_ss_io}]

###############################################################################
# NanoEVB, PicoEVB common I/O
###############################################################################

set_property PACKAGE_PIN V14 [get_ports {user_led[2]}]
set_property PACKAGE_PIN V13 [get_ports {user_led[1]}]
set_property PACKAGE_PIN V12 [get_ports {user_led[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {user_led[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {user_led[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {user_led[0]}]
set_property PULLUP true [get_ports {user_led[2]}]
set_property PULLUP true [get_ports {user_led[1]}]
set_property PULLUP true [get_ports {user_led[0]}]
set_property DRIVE 8 [get_ports {user_led[2]}]
set_property DRIVE 8 [get_ports {user_led[1]}]
set_property DRIVE 8 [get_ports {user_led[0]}]

# clkreq_l is active low clock request for M.2 card to
# request PCI Express reference clock
set_property PACKAGE_PIN A9 [get_ports clkreq_l]
set_property IOSTANDARD LVCMOS33 [get_ports clkreq_l]
set_property PULLDOWN true [get_ports clkreq_l]

# Auxillary I/O Connector
set_property PACKAGE_PIN A14 [get_ports uart_rxd]
set_property PACKAGE_PIN A13 [get_ports uart_txd]
set_property IOSTANDARD LVCMOS33 [get_ports uart_rxd]
set_property IOSTANDARD LVCMOS33 [get_ports uart_txd]
set_property PACKAGE_PIN B12 [get_ports user_io[0]]
set_property PACKAGE_PIN A12 [get_ports user_io[1]]
set_property IOSTANDARD LVCMOS33 [get_ports user_io[0]]
set_property IOSTANDARD LVCMOS33 [get_ports user_io[1]]

###############################################################################
# Additional design / project settings
###############################################################################

# Power down on overtemp
set_property BITSTREAM.CONFIG.OVERTEMPPOWERDOWN ENABLE [current_design]

# High-speed configuration so FPGA is up in time to negotiate with PCIe root complex
set_property BITSTREAM.CONFIG.CONFIGRATE 66 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property CONFIG_MODE SPIx4 [current_design]
set_property BITSTREAM.CONFIG.SPI_FALL_EDGE YES [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]

set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property CFGBVS VCCO [current_design]
