#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#define PROT_CTL_PING             0
#define PROT_CTL_RESET            1
#define PROT_CTL_STATUS           2
#define PROT_CTL_TLP_SEND         3
#define PROT_CTL_TLP_RECV         4
#define PROT_CTL_SUCCESS          5
#define PROT_CTL_ERROR_FAILED     6
#define PROT_CTL_ERROR_TIMEOUT    7
#define PROT_CTL_CONFIG           8
#define PROT_CTL_TEST             9
#define PROT_CTL_RESIDENT_ON      10
#define PROT_CTL_RESIDENT_OFF     11
#define PROT_CTL_ROM_WRITE        12
#define PROT_CTL_ROM_ERASE        13
#define PROT_CTL_ROM_LOG_ON       14
#define PROT_CTL_ROM_LOG_OFF      15
#define PROT_CTL_ROM_SIZE         16

/* used only by this particular design */
#define PROT_CTL_BIT_WRITE        0xfe
#define PROT_CTL_BIT_ERASE        0xff

typedef struct _PROT_CTL
{
    u8 code; // see PROT_CTL_*
    u8 size;
    u8 data[];

} __attribute__((packed)) PROT_CTL;

typedef struct _PROT_CTL_ROM
{
    u32 offset;
    u8 data[];

} __attribute__((packed)) PROT_CTL_ROM;

#define PROT_MAX_PACKET_SIZE 0x100

#endif
