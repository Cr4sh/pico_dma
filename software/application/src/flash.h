#ifndef __FLASH_H_
#define __FLASH_H_

/*
    Number of bytes per page, sector and block in the flash device.
 */
#define FLASH_PAGE_SIZE     0x100
#define FLASH_SECTOR_SIZE   0x1000  // 4Kb
#define FLASH_BLOCK_SIZE    0x10000 // 64Kb

/*
    Definitions of the SPI flash commands.
 */
#define COMMAND_STATUS_WRITE    0x01    // status registers write command
#define COMMAND_STATUS_READ_1   0x05    // SR1 read command
#define COMMAND_STATUS_READ_2   0x35    // SR2 read command
#define COMMAND_STATUS_READ_3   0x33    // SR3 read command
#define COMMAND_PAGE_PROGRAM    0x02    // page program command
#define COMMAND_QUAD_WRITE      0x32    // quad input fast program
#define COMMAND_RANDOM_READ     0x03    // random read command
#define COMMAND_DUAL_READ       0x3B    // dual output fast read
#define COMMAND_DUAL_IO_READ    0xBB    // dual IO fast read
#define COMMAND_QUAD_READ       0x6B    // quad output fast read
#define COMMAND_QUAD_IO_READ    0xEB    // quad IO fast read
#define COMMAND_WRITE_ENABLE    0x06    // write enable command
#define COMMAND_WRITE_DISABLE   0x04    // write disable command
#define COMMAND_SECTOR_ERASE    0x20    // sector erase command
#define COMMAND_BLOCK_ERASE     0xD8    // block erase command
#define COMMAND_BULK_ERASE      0xC7    // bulk erase command

int spi_flash_initialize(void);

int spi_flash_write_enable(u8 Command);
int spi_flash_block_erase(u32 Address);

int spi_flash_read(u32 Address, u8 *Buff, u32 Size, u8 Command);
int spi_flash_write(u32 Address, u8 *Buff, u32 Size);

#endif
