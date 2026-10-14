#ifndef PCCF_PROTOCOL_H
#define PCCF_PROTOCOL_H
#define PCCF_DEVCTL_OFF 0x50434601
#define PCCF_DEVCTL_INFO 0x50434602
typedef struct {
    unsigned int online, sectors, sector_reads, fat_type;
} pccf_info_t;
#endif
