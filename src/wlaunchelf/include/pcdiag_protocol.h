#ifndef PCDIAG_PROTOCOL_H
#define PCDIAG_PROTOCOL_H
#define PCDIAG_RPC_ID 0x50434331
#define PCDIAG_MAGIC 0x50434331
#define PCDIAG_IOP_CAP 32768
typedef struct {
    unsigned int magic, used, dropped, offset, count, flags;
    char data[1000];
} pcdiag_packet_t;
#endif
