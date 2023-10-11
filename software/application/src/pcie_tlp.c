#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <xil_printf.h>
#include <xil_types.h>
#include <xstatus.h>

#include "platform_config.h"
#include "axi_dma.h"
#include "protocol.h"
#include "pcie_tlp.h"
#include "common.h"

// DMA buffers for transmit and receive
static u8 *m_buff_rx = (u8 *)(DMA_BUFF_ADDR + 0);
static u8 *m_buff_tx = (u8 *)(DMA_BUFF_ADDR + PROT_MAX_PACKET_SIZE);

// defined in application.c
extern u32 m_dev_id;

int tlp_size(u8 *data)
{
    u32 header = 0, type = 0, size = 0;

    memcpy(&header, data, sizeof(u32));

    type = (header >> 29) & 0x3;
    size = (header >> 0) & 0x3ff;

    switch (type)
    {
    case TLP_3_NO_DATA:

        return sizeof(u32) * 3;

    case TLP_4_NO_DATA:
 
        return sizeof(u32) * 4;

    case TLP_3_DATA:

        return sizeof(u32) * (3 + size);

    case TLP_4_DATA:

        return sizeof(u32) * (4 + size);
    }

    return 0;
}

int tlp_recv(u8 *buff, u32 *ret_size)
{
    u32 size = 0;

    // receive TLP
    if (axi_dma_queue_rx(m_buff_rx, PROT_MAX_PACKET_SIZE, NULL) == XST_SUCCESS)
    {
        // wait for the completion
        if (axi_dma_wait_rx(TLP_READ_TIMEOUT) == XST_SUCCESS)
        {

#if (XPAR_MICROBLAZE_USE_DCACHE != 0)

            // invalidate cache after the transfer
            Xil_DCacheInvalidateRange((u32)m_buff_rx, PROT_MAX_PACKET_SIZE);
#endif
            // calculate TLP length
            if ((size = tlp_size(m_buff_rx)) != 0)
            {
                if (buff)
                {
                    // copy TLP data to the DMA transmit buffer
                    memcpy(buff, m_buff_rx, size);
                }

                if (ret_size)
                {
                    *ret_size = size;
                }

                return XST_SUCCESS;
            }
        }
        else
        {
            return XST_TIMEOUT;
        }
    }

    return XST_FAILURE;
}

int tlp_send(u8 *buff, u32 size)
{
    if (buff)
    {
        // copy TLP data to the DMA transmit buffer
        memcpy(m_buff_tx, buff, size);
    }

#if (XPAR_MICROBLAZE_USE_DCACHE != 0)

    // flush cache before the transfer
    Xil_DCacheFlushRange((u32)m_buff_tx, PROT_MAX_PACKET_SIZE);

#endif

    // send TLP
    if (axi_dma_queue_tx(m_buff_tx, size, NULL) == XST_SUCCESS)
    {
        // wait for the completion
        axi_dma_wait_tx(0);

        return XST_SUCCESS;
    }

    return XST_FAILURE;
}

int mem_read(u64 addr, u8 *buff, u32 size)
{
    u32 ptr = 0, tlp_size = 0;
    u32 *tlp_tx = (u32 *)m_buff_tx, *tlp_rx = (u32 *)m_buff_rx;    

    if (addr % sizeof(u32) != 0)
    {
        xil_printf("mem_read() ERROR: address is not aligned\n");
        return XST_FAILURE;
    }
    
    while (ptr < size)
    {        
        // construct memory read TLP
        tlp_tx[0] = (TLP_TYPE_MRd64 << 24) | 1; // TLP type and data size
        tlp_tx[1] = (m_dev_id << 16) | 0xff;    // requester ID and byte enable flags
        tlp_tx[2] = (u32)(addr >> 32);          // high dword of physical memory address
        tlp_tx[3] = (u32)(addr & 0xffffffff);   // low dword of physical memory address

        // send request
        if (tlp_send(NULL, sizeof(u32) * 4) != XST_SUCCESS)
        {
            return XST_FAILURE;
        }

        // receive reply
        if (tlp_recv(NULL, &tlp_size) != XST_SUCCESS)
        {
            return XST_FAILURE;
        }

        // check for the valid completion TLP
        if (tlp_size != 4 && (tlp_rx[0] >> 24) != TLP_TYPE_CplD)
        {
            return XST_FAILURE;
        }

        // get completion data
        u8 *data = (u8 *)&tlp_rx[3];

        // swap endianess
        u32 val = data[3] | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);

        // copy data to the output buffer
        memcpy(buff + ptr, &val, MIN(sizeof(u32), size - ptr));

        addr += sizeof(u32);
        ptr += sizeof(u32);
    }

    return XST_SUCCESS;
}

int mem_write(u64 addr, u8 *buff, u32 size)
{
    u32 ptr = 0;
    u32 *tlp_tx = (u32 *)m_buff_tx;    

    if (addr % sizeof(u32) != 0)
    {
        xil_printf("mem_read() ERROR: address is not aligned\n");
        return XST_FAILURE;
    }

    if (size % sizeof(u32) != 0)
    {
        xil_printf("mem_read() ERROR: size is not aligned\n");
        return XST_FAILURE;
    }

    while (ptr < size)
    {
        // get data to write
        u8 *data = buff + ptr;

        // swap endianess
        u32 val = data[3] | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);

        // construct memory write TLP
        tlp_tx[0] = (TLP_TYPE_MWr64 << 24) | 1; // TLP type and data size
        tlp_tx[1] = (m_dev_id << 16) | 0xff;    // requester ID and byte enable flags
        tlp_tx[2] = (u32)(addr >> 32);          // high dword of physical memory address
        tlp_tx[3] = (u32)(addr & 0xffffffff);   // low dword of physical memory address
        tlp_tx[4] = val;                        // data

        // send request
        if (tlp_send(NULL, sizeof(u32) * 5) != XST_SUCCESS)
        {
            return XST_FAILURE;
        }

        addr += sizeof(u32);
        ptr += sizeof(u32);
    }

    return XST_SUCCESS;
}
