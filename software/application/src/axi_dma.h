#ifndef __AXI_DMA_H_
#define __AXI_DMA_H_

typedef void (* AXI_DMA_HANDLER)(void);

void axi_dma_reset(void);

int axi_dma_initialize(void);

int axi_dma_queue_tx(void *buff, u32 size, AXI_DMA_HANDLER handler);
int axi_dma_queue_rx(void *buff, u32 size, AXI_DMA_HANDLER handler);

bool axi_dma_queued_tx(void);
bool axi_dma_queued_rx(void);

int axi_dma_wait_tx(u32 timeout);
int axi_dma_wait_rx(u32 timeout);

#endif
