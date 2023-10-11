#include <stdio.h>
#include <fsl.h>

#include "axis_pcie.h"

u32 axis_pcie_read_config(u32 num)
{
    u32 val;

    getfslx(val, AXIS_PCIE_DEV_ID_CONFIG, FSL_NONBLOCKING);

    // write config space register number
    putfslx(num, AXIS_PCIE_DEV_ID_CONFIG, FSL_DEFAULT);
    putfslx(num, AXIS_PCIE_DEV_ID_CONFIG, FSL_CONTROL);

    // read config space register data
    getfslx(val, AXIS_PCIE_DEV_ID_CONFIG, FSL_DEFAULT);

    return val;
}
