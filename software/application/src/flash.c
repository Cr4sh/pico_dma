#include <stdio.h>
#include <stdbool.h>

#include <xil_printf.h>
#include <xil_types.h>
#include <xparameters.h>
#include <xspi.h>

#include "platform_config.h"
#include "flash.h"

/*
    The following constant defines the slave select signal that is used to
    to select the Flash device on the SPI bus, this signal is typically
    connected to the chip select of the device.
 */
#define SPI_SELECT 0x01

#define READ_WRITE_EXTRA_BYTES 4

// SPI device instance
static XSpi m_Spi;

// read/write buffer
static u8 m_buff_tx[READ_WRITE_EXTRA_BYTES + FLASH_PAGE_SIZE];
static u8 m_buff_rx[READ_WRITE_EXTRA_BYTES + FLASH_PAGE_SIZE + 4];

static int spi_transfer(u8 *buff_tx, u8 *buff_rx, u32 size)
{
    int Status = XST_FAILURE;   

    // start the SPI driver so that the device is enabled
    XSpi_Start(&m_Spi);

    // disable global interrupt to use polled mode operation
    XSpi_IntrGlobalDisable(&m_Spi);    

    if ((Status = XSpi_SetSlaveSelect(&m_Spi, SPI_SELECT)) != XST_SUCCESS) 
    {

#ifdef VERBOSE

        xil_printf("ERROR: XSpi_SetSlaveSelect() fails\n");
#endif
        return XST_FAILURE;
    }

    // perform SPI transfer
    if ((Status = XSpi_Transfer(&m_Spi, buff_tx, buff_rx, size)) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XSpi_Transfer() fails\n");
#endif
        return XST_FAILURE;
    }   

    XSpi_Stop(&m_Spi);

    return XST_SUCCESS;
}

static int spi_flash_get_status(u8 *Val)
{
    int Status = XST_FAILURE;
    
    m_buff_tx[0] = COMMAND_STATUS_READ_1;

    // send SR1 read command
    if ((Status = spi_transfer(m_buff_tx, m_buff_rx, 2)) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }   

    if (Val)
    {
        *Val = m_buff_rx[1];
    }

    return XST_SUCCESS;
}

static int spi_flash_wait_ready(void)
{   
    int Status = XST_FAILURE; 

    while (true)
    {
        u8 Val = 0;

        // get status 1 register value
        if ((Status = spi_flash_get_status(&Val)) != XST_SUCCESS)
        {
            return XST_FAILURE;
        }   

        // check if flash is ready to accept next command
        if ((Val & 0x01) == 0) 
        {
            break;
        }
    }

    return XST_SUCCESS;
}

int spi_flash_write_enable(u8 Command)
{
    int Status = XST_FAILURE;

    if ((Status = spi_flash_wait_ready()) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: spi_flash_wait_ready() fails\n");
#endif
        return XST_FAILURE;
    }

    m_buff_tx[0] = Command;
    m_buff_tx[1] = 0;

    // send write enable command
    if ((Status = spi_transfer(m_buff_tx, NULL, 1)) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

int spi_flash_block_erase(u32 Address)
{
    int Status = XST_FAILURE;    

    if ((Status = spi_flash_wait_ready()) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: spi_flash_wait_ready() fails\n");
#endif
        return XST_FAILURE;
    }

    // prepare sector erase command
    m_buff_tx[0] = COMMAND_BLOCK_ERASE;
    m_buff_tx[1] = (u8)((Address >> 16) & 0xff);
    m_buff_tx[2] = (u8)((Address >> 8) & 0xff);
    m_buff_tx[3] = (u8)((Address >> 0) & 0xff);    

    // send command
    if ((Status = spi_transfer(m_buff_tx, NULL, 4)) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

int spi_flash_read(u32 Address, u8 *Buff, u32 Size, u8 Command)
{
    int Status = XST_FAILURE;
    u32 DummyBytes = 0;

    if (Size > FLASH_PAGE_SIZE)
    {
        // read size is too large
        return XST_FAILURE;
    }

    if ((Status = spi_flash_wait_ready()) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: spi_flash_wait_ready() fails\n");
#endif
        return XST_FAILURE;
    }
    
    // prepare flash read command
    m_buff_tx[0] = Command;
    m_buff_tx[1] = (u8)((Address >> 16) & 0xff);
    m_buff_tx[2] = (u8)((Address >> 8) & 0xff);
    m_buff_tx[3] = (u8)((Address >> 0) & 0xff);    

    // dummy clock cycles before data available
    if (Command == COMMAND_DUAL_READ) 
    {
        DummyBytes = 2;
    } 
    else if (Command == COMMAND_DUAL_IO_READ) 
    {
        DummyBytes = 1;
    } 
    else if (Command == COMMAND_QUAD_IO_READ) 
    {
        DummyBytes = 3;
    } 
    else if (Command == COMMAND_QUAD_READ) 
    {
        DummyBytes = 4;
    }

    // perform transfer
    if ((Status = spi_transfer(m_buff_tx, m_buff_rx, Size + DummyBytes + READ_WRITE_EXTRA_BYTES)) != XST_SUCCESS) 
    {
        return XST_FAILURE;
    }

    if (Buff)
    {
        // copy data to the caller buffer
        memcpy(Buff, m_buff_rx + DummyBytes + READ_WRITE_EXTRA_BYTES, Size);
    }

    return XST_SUCCESS;
}

int spi_flash_write(u32 Address, u8 *Buff, u32 Size)
{
    int Status = XST_FAILURE;

    if (Size > FLASH_PAGE_SIZE)
    {
        // write size is too large
        return XST_FAILURE;
    }

    if ((Status = spi_flash_wait_ready()) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: spi_flash_wait_ready() fails\n");
#endif
        return XST_FAILURE;
    }

    // prepare flash write command
    m_buff_tx[0] = COMMAND_PAGE_PROGRAM;
    m_buff_tx[1] = (u8)((Address >> 16) & 0xff);
    m_buff_tx[2] = (u8)((Address >> 8) & 0xff);
    m_buff_tx[3] = (u8)((Address >> 0) & 0xff);    

    // copy data from the caller buffer
    memcpy(m_buff_tx + READ_WRITE_EXTRA_BYTES, Buff, Size);    

    // perform transfer
    if ((Status = spi_transfer(m_buff_tx, NULL, Size + READ_WRITE_EXTRA_BYTES)) != XST_SUCCESS) 
    {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}   

int spi_flash_initialize(void)
{
    int Status = XST_FAILURE;

    // initialize the SPI driver
    if ((Status = XSpi_Initialize(&m_Spi, DEVICE_ID_SPI)) != XST_SUCCESS) 
    {

#ifdef VERBOSE

        xil_printf("ERROR: XSpi_Initialize() fails\n");
#endif
        return XST_FAILURE;
    }        

    // set SPI master mode
    if ((Status = XSpi_SetOptions(&m_Spi, XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION)) != XST_SUCCESS)
    {

#ifdef VERBOSE

        xil_printf("ERROR: XSpi_SetOptions() fails\n");
#endif
        return XST_FAILURE;  
    }

    return XST_SUCCESS;
}
