#ifndef __EFI_H_
#define __EFI_H_

typedef u64 efi_ptr;

typedef efi_ptr efi_handle;

struct efi_table_header
{
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc32;
    u32 reserved;
};

struct efi_system_table
{
    struct efi_table_header hdr;

    efi_ptr firmware_vendor;
    u64 firmware_revision;

    efi_handle console_in_handler;
    efi_ptr con_in;
    
    efi_handle console_out_handler;
    efi_ptr con_out;
    
    efi_handle standard_error_handle;
    efi_ptr std_err;
    
    efi_ptr runtime_services;
    efi_ptr boot_services;
    u64 num_table_entries;
    efi_ptr configuration_table;
};

struct efi_boot_services
{
    struct efi_table_header hdr;

    /* Task Priority Services */
    efi_ptr raise_tpl;
    efi_ptr restore_tpl;

    /* Memory Services */
    efi_ptr allocate_pages;
    efi_ptr free_pages;
    efi_ptr get_memory_map;
    efi_ptr allocate_pool;
    efi_ptr free_pool;

    /* Event & Timer Services */
    efi_ptr create_event;
    efi_ptr set_timer;
    efi_ptr wait_for_event;
    efi_ptr signal_event;
    efi_ptr close_event;
    efi_ptr check_event;

    /* Protocol Handler Services */
    efi_ptr install_protocol_interface;
    efi_ptr reinstall_protocol_interface;
    efi_ptr uninstall_protocol_interface;
    efi_ptr handle_protocol;
    efi_ptr reserved;
    efi_ptr register_protocol_notify;
    efi_ptr locate_handle;
    efi_ptr locate_device_path;
    efi_ptr install_configuration_table;

    /* Image Services */
    efi_ptr load_image;
    efi_ptr start_image;
    efi_ptr exit;
    efi_ptr unload_image;
    efi_ptr exit_boot_services;

    /* Miscellaneous Services */
    efi_ptr get_next_monotonic_count;
    efi_ptr stall;
    efi_ptr set_watchdog_timer;

    /* DriverSupport Services */
    efi_ptr connect_controller;
    efi_ptr disconnect_controller;

    /* Open and Close Protocol Services */
    efi_ptr open_protocol;
    efi_ptr close_protocol;
    efi_ptr open_protocol_information;

    /* Library Services */
    efi_ptr protocols_per_handle;
    efi_ptr locate_handle_buffer;
    efi_ptr locate_protocol;
    efi_ptr install_multiple_protocol_interfaces;
    efi_ptr uninstall_multiple_protocol_interfaces;

    /* 32-bit CRC Services */
    efi_ptr calculate_crc32;

    /* Miscellaneous Services */
    efi_ptr copy_mem;
    efi_ptr set_mem;
};

#endif
