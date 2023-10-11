#ifndef __PE_IMAGE_H_
#define __PE_IMAGE_H_

// DOS header signature
#define DOS_SIGNATURE 0x5a4d

// NT headers signature
#define PE_SIGNATURE 0x4550

struct dos_header
{
    u16 e_magic;
    u16 e_cblp;
    u16 e_cp;
    u16 e_crlc;
    u16 e_cparhdr;
    u16 e_minalloc;
    u16 e_maxalloc;
    u16 e_ss;
    u16 e_sp;
    u16 e_csum;
    u16 e_ip;
    u16 e_cs;
    u16 e_lfarlc;
    u16 e_ovno;
    u16 e_res[4];
    u16 e_oemid;
    u16 e_oeminfo;
    u16 e_res2[10];
    u32 e_lfanew;
};

struct pe32_file_header
{
    u16 machine;
    u16 num_sections;
    u32 time;
    u32 symtab_offset;
    u32 num_symbols;
    u16 optional_header_size;
    u16 characteristics;
};

/*
    Machine type constants
*/
#define PE32_MACHINE_I386       0x014c
#define PE32_MACHINE_IA64       0x0200
#define PE32_MACHINE_X86_64     0x8664
#define PE32_MACHINE_ARM64      0xaa64

/*
    Subsystem constnts
*/
#define SUBSYSTEM_EFI_APPLICATION           10
#define SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER   11
#define SUBSYSTEM_EFI_RUNTIME_DRIVER        12
#define SUBSYSTEM_EFI_ROM                   13

struct pe32_data_directory
{
    u32 rva;
    u32 size;
};

struct pe64_optional_header
{
    u16 magic;
    u8 major_linker_version;
    u8 minor_linker_version;
    u32 code_size;
    u32 data_size;
    u32 bss_size;
    u32 entry_addr;
    u32 code_base;

    u64 image_base;

    u32 section_alignment;
    u32 file_alignment;
    u16 major_os_version;
    u16 minor_os_version;
    u16 major_image_version;
    u16 minor_image_version;
    u16 major_subsystem_version;
    u16 minor_subsystem_version;
    u32 reserved;
    u32 image_size;
    u32 header_size;
    u32 checksum;
    u16 subsystem;
    u16 dll_characteristics;

    u64 stack_reserve_size;
    u64 stack_commit_size;
    u64 heap_reserve_size;
    u64 heap_commit_size;

    u32 loader_flags;
    u32 num_data_directories;

    /* Data directories */
    struct pe32_data_directory export_table;
    struct pe32_data_directory import_table;
    struct pe32_data_directory resource_table;
    struct pe32_data_directory exception_table;
    struct pe32_data_directory certificate_table;
    struct pe32_data_directory base_relocation_table;
    struct pe32_data_directory debug;
    struct pe32_data_directory architecture;
    struct pe32_data_directory global_ptr;
    struct pe32_data_directory tls_table;
    struct pe32_data_directory load_config_table;
    struct pe32_data_directory bound_import;
    struct pe32_data_directory iat;
    struct pe32_data_directory delay_import_descriptor;
    struct pe32_data_directory com_runtime_header;
    struct pe32_data_directory reserved_entry;
};

struct pe64_nt_headers
{
    /* This is always PE\0\0 */
    u32 signature;

    /* The COFF file header */
    struct pe32_file_header file_header;

    /* The Optional header */
    struct pe64_optional_header optional_header;
};

struct pe32_section_header
{
    char name[8];
    u32 virtual_size;
    u32 virtual_address;
    u32 raw_data_size;
    u32 raw_data_offset;
    u32 relocations_offset;
    u32 line_numbers_offset;
    u16 num_relocations;
    u16 num_line_numbers;
    u32 characteristics;
};

#endif
