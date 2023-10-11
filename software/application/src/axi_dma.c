#include <stdio.h>
#include <stdbool.h>

#include <xil_printf.h>
#include <xil_types.h>
#include <xil_exception.h>
#include <xil_cache.h>
#include <xparameters.h>
#include <xaxidma.h>
#include <xintc.h>

#include "platform_config.h"
#include "axi_dma.h"

// DMA engine instance
static XAxiDma m_AxiDma;

// interrupt controller instance
static XIntc m_Intc;

// user handlers
static AXI_DMA_HANDLER m_handler_rx = NULL;
static AXI_DMA_HANDLER m_handler_tx = NULL;

static int m_queued_tx = 0, m_queued_rx = 0;

static void axi_dma_interrupts_disable(void)
{
    // disable all interrupts
    XAxiDma_IntrDisable(&m_AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
    XAxiDma_IntrDisable(&m_AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
}

static void axi_dma_interrupts_enable(void)
{
    // enable all interrupts
    XAxiDma_IntrEnable(&m_AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
    XAxiDma_IntrEnable(&m_AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
}

void axi_dma_reset(void)
{
    m_queued_rx = m_queued_tx = 0;
    m_handler_rx = m_handler_tx = NULL;    

    XAxiDma_Reset(&m_AxiDma);

    while (XAxiDma_ResetIsDone(&m_AxiDma) == 0)
    {
        // wait for reset
    }
   
    axi_dma_interrupts_disable();
    axi_dma_interrupts_enable();  
}

void axi_dma_isr_tx(void *Param)
{
    XAxiDma *AxiDma = (XAxiDma *)Param;

    // read pending interrupts
    u32 IrqStatus = XAxiDma_IntrGetIrq(AxiDma, XAXIDMA_DMA_TO_DEVICE);

    // acknowledge pending interrupts
    XAxiDma_IntrAckIrq(AxiDma, IrqStatus, XAXIDMA_DMA_TO_DEVICE);

    if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK))
    {
        // no interrupt is asserted
        return;
    }

    // check for the error
    if (IrqStatus & XAXIDMA_IRQ_ERROR_MASK)
    {

#ifdef VERBOSE

        xil_printf("isr_dma_tx(): IRQ error\n");
#endif
        // reset DMA engine
        axi_dma_reset();
        return;
    }

    // transmit completed
    if (IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))
    {

#ifdef VERBOSE

        xil_printf("isr_dma_tx(): transmit completed\n");
#endif
        m_queued_tx = 0;

        if (m_handler_tx)
        {
            // call user handler
            m_handler_tx();
            m_handler_tx = NULL;            
        }
    }
}

void axi_dma_isr_rx(void *Param)
{
    XAxiDma *AxiDma = (XAxiDma *)Param;

    // read pending interrupts
    u32 IrqStatus = XAxiDma_IntrGetIrq(AxiDma, XAXIDMA_DEVICE_TO_DMA);

    // acknowledge pending interrupts
    XAxiDma_IntrAckIrq(AxiDma, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

    if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK))
    {
        // no interrupt is asserted
        return;
    }

    // check for the error
    if (IrqStatus & XAXIDMA_IRQ_ERROR_MASK)
    {

#ifdef VERBOSE

        xil_printf("isr_dma_rx(): IRQ error\n");
#endif
        // reset DMA engine
        axi_dma_reset();
        return;
    }

    if (IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))
    {

#ifdef VERBOSE

        xil_printf("isr_dma_rx(): receive completed\n");
#endif
        m_queued_rx = 0;

        if (m_handler_rx)
        {
            // call user handler
            m_handler_rx();
            m_handler_rx = NULL;            
        }
    }
}

int axi_dma_interrupts_init(XIntc *Intc, XAxiDma *AxiDma)
{   
    // initialize interrupt controller and connect the ISRs
    int Status = XIntc_Initialize(Intc, DEVICE_ID_INTC);
    if (Status != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XIntc_Initialize() fails\n");
#endif
        return XST_FAILURE;
    }

    // register transmit ISR
    if ((Status = XIntc_Connect(Intc, VEC_ID_AXI_DMA_MM2S, axi_dma_isr_tx, AxiDma)) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XIntc_Connect() fails\n");

#endif
        return XST_FAILURE;
    }

    // register receive ISR
    if ((Status = XIntc_Connect(Intc, VEC_ID_AXI_DMA_S2MM, axi_dma_isr_rx, AxiDma)) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XIntc_Connect() fails\n");
#endif
        return XST_FAILURE;
    }

    // start interrupt controller
    if ((Status = XIntc_Start(Intc, XIN_REAL_MODE)) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XIntc_Start() fails\n");
#endif
        return XST_FAILURE;
    }

    // enable interrupts
    XIntc_Enable(Intc, VEC_ID_AXI_DMA_MM2S);
    XIntc_Enable(Intc, VEC_ID_AXI_DMA_S2MM);

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XIntc_InterruptHandler, Intc);
    Xil_ExceptionEnable();

    return XST_SUCCESS;
}

int axi_dma_initialize(void)
{
    m_queued_rx = m_queued_tx = 0;
    m_handler_rx = m_handler_tx = NULL;    

#ifdef VERBOSE

    xil_printf("Initializing DMA engine...\n");

#endif

    XAxiDma_Config *Config = XAxiDma_LookupConfig(DEVICE_ID_AXI_DMA);
    if (Config == NULL)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XAxiDma_LookupConfig() fails\n");
#endif
        return XST_FAILURE;
    }

    XAxiDma_CfgInitialize(&m_AxiDma, Config);

    // check for Scatter-Gather mode
    if (XAxiDma_HasSg(&m_AxiDma))
    {

#ifdef VERBOSE

        xil_printf("ERROR: Scatter-Gather DMA is configured\n");
#endif
        return XST_FAILURE;
    }

#ifdef VERBOSE

    xil_printf("Initializing interrupts controller...\n");

#endif

    // set up interrupt controller
    if (axi_dma_interrupts_init(&m_Intc, &m_AxiDma) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    axi_dma_interrupts_disable();
    axi_dma_interrupts_enable();

    return XST_SUCCESS;
}

int axi_dma_queue_tx(void *buff, u32 size, AXI_DMA_HANDLER handler)
{
    if (m_queued_tx != 0)
    {

#ifdef VERBOSE

        xil_printf("axi_dma_queue_tx() ERROR: Busy\n");
#endif

        return XST_FAILURE;
    }

    m_queued_tx += 1;
    m_handler_tx = handler;    

    // start transmit transfer
    if (XAxiDma_SimpleTransfer(&m_AxiDma, (u32)buff, size, XAXIDMA_DMA_TO_DEVICE) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XAxiDma_SimpleTransfer() fails\n");
#endif

        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

int axi_dma_queue_rx(void *buff, u32 size, AXI_DMA_HANDLER handler)
{
    if (m_queued_rx != 0)
    {

#ifdef VERBOSE

        xil_printf("axi_dma_queue_rx() ERROR: Busy\n");
#endif

        return XST_FAILURE;
    }

    m_queued_rx += 1;
    m_handler_rx = handler;

    // start receive transfer
    if (XAxiDma_SimpleTransfer(&m_AxiDma, (u32)buff, size, XAXIDMA_DEVICE_TO_DMA) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XAxiDma_SimpleTransfer() fails\n");
#endif

        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

bool axi_dma_queued_tx(void)
{
    return m_queued_tx != 0;
}

bool axi_dma_queued_rx(void)
{
    return m_queued_rx != 0;
}

int axi_dma_wait_tx(u32 timeout)
{
    u32 counter = 0;

    while (axi_dma_queued_tx())
    {
        if (timeout != 0)
        {
            // wait with timeout
            if (counter >= timeout)
            {

#ifdef VERBOSE
                xil_printf("axi_dma_wait_tx() ERROR: Timeout occurred\n");
#endif
                // reset DMA engine
                axi_dma_reset();

                return XST_FAILURE;
            }

            counter += 1;
        }
    }

    return XST_SUCCESS;
}

int axi_dma_wait_rx(u32 timeout)
{
    u32 counter = 0;

    while (axi_dma_queued_rx())
    {
        if (timeout != 0)
        {
            // wait with timeout
            if (counter >= timeout)
            {

#ifdef VERBOSE
                xil_printf("axi_dma_wait_rx() ERROR: Timeout occurred\n");
#endif
                // reset DMA engine
                axi_dma_reset();

                return XST_FAILURE;
            }

            counter += 1;
        }
    }

    return XST_SUCCESS;
}
