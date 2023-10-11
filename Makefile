project:
	vivado -mode batch -source 7x_pcie_microblaze.tcl

bin:
	cp 7x_pcie_microblaze/7x_pcie_microblaze.runs/impl_1/pcie_microblaze_top.bin 7x_pcie_microblaze.bin
	cp 7x_pcie_microblaze/7x_pcie_microblaze.runs/impl_1/pcie_microblaze_top.bit 7x_pcie_microblaze.bit

