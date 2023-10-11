#include <xparameters.h>
#include <xil_cache.h>

#include "platform_config.h"

void enable_caches(void)
{

#if (XPAR_MICROBLAZE_USE_ICACHE != 0)

    Xil_ICacheEnable();

#endif

#if (XPAR_MICROBLAZE_USE_DCACHE != 0)

    Xil_DCacheEnable();

#endif

}

void disable_caches(void)
{

#if (XPAR_MICROBLAZE_USE_DCACHE != 0)

    Xil_DCacheDisable();

#endif

#if (XPAR_MICROBLAZE_USE_ICACHE != 0)

    Xil_ICacheDisable();

#endif

}

void init_uart(void)
{
    // bootrom/BSP configures PS7/PSU UART to 115200 bps
}

void init_platform(void)
{    
    enable_caches();

    init_uart();
}

void cleanup_platform(void)
{
    disable_caches();
}
