#ifndef __AXIS_PCIE_H_
#define __AXIS_PCIE_H_

// ID of AXI stream device used to access PCI-E config space
#define AXIS_PCIE_DEV_ID_CONFIG 0

u32 axis_pcie_read_config(u32 num);

#endif
