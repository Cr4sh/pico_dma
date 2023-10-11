#ifndef __PLATFORM_CONFIG_H_
#define __PLATFORM_CONFIG_H_

#define USE_ROM

// device identifiers
#define DEVICE_ID_UART XPAR_AXI_UARTLITE_0_DEVICE_ID
#define DEVICE_ID_GPIO XPAR_AXI_GPIO_0_DEVICE_ID
#define DEVICE_ID_INTC XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID
#define DEVICE_ID_AXI_DMA XPAR_AXI_DMA_0_DEVICE_ID
#define DEVICE_ID_SPI XPAR_AXI_QUAD_SPI_0_DEVICE_ID

// interrupt vectors
#define VEC_ID_AXI_DMA_MM2S XPAR_INTC_0_AXIDMA_0_MM2S_INTROUT_VEC_ID
#define VEC_ID_AXI_DMA_S2MM XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define VEC_ID_SPI XPAR_INTC_0_SPI_0_VEC_ID

// DMA buffer base address
#define DMA_BUFF_ADDR XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR

// TLP receive timeout
#define TLP_READ_TIMEOUT 1000000

// address and size of the payload ROM on the SPI flash
#define ROM_SPI_ADDR 0x150000
#define ROM_SPI_SIZE 0x2b0000

// address and size of the FPGA bitstream on the SPI flash
#define BIT_SPI_ADDR 0
#define BIT_SPI_SIZE 0x150000

#endif
