/* Actual ATA/FAT code with a stateful PIO card: transfer count, widths,
   byte order, command whitelist, timeouts and failures are observable. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../iop/pcdev9/pcdiag_ata.h"

enum { OK, BAD_MAP, REJECT_COMMAND, DRQ_TIMEOUT, DRQ_ERROR, DRQ_FAULT,
       FLOATING, COMPLETE_TIMEOUT, COMPLETE_ERROR, FINAL_ERROR };
typedef struct {
    unsigned int space, width, commands, reads, words, waits, position, alternate;
    unsigned int command, lba, fault, fault_command, fat, full_root, delay;
    unsigned char id[512], mbr[512], boot[512], root[512];
} card_t;
static card_t card;
static char log_buffer[32768];
static unsigned int log_used;
static void trace(const char *format, ...)
{
    va_list ap;
    char line[384];
    int count;
    va_start(ap, format); count = vsnprintf(line, sizeof(line), format, ap); va_end(ap);
    assert(count >= 0 && count < (int)sizeof(line));
    assert(log_used + (unsigned int)count + 1 < sizeof(log_buffer));
    memcpy(log_buffer + log_used, line, count);
    log_used += count; log_buffer[log_used++] = '\n'; log_buffer[log_used] = 0;
}
static void put16(unsigned char *p, unsigned int n) { p[0] = n; p[1] = n >> 8; }
static void put32(unsigned char *p, unsigned int n) { put16(p, n); put16(p + 2, n >> 16); }
static void ata_string(unsigned int word, unsigned int size, const char *s)
{
    unsigned int i, len = strlen(s);
    for (i = 0; i < size; ++i) card.id[word * 2 + (i ^ 1U)] = i < len ? s[i] : ' ';
}
static void reset(void)
{
    memset(&card, 0, sizeof(card)); log_used = 0; log_buffer[0] = 0;
    card.fault_command = 1;
    put16(card.id, 0x848a); put16(card.id + 98, 0x0200); put32(card.id + 120, 12288);
    ata_string(27, 40, "TRANSCEND TS4GCF133"); ata_string(10, 20, "SERIAL123"); ata_string(23, 8, "V1.0");
}
static void make_fat(int full)
{
    unsigned int i;
    card.fat = 1; card.full_root = full;
    card.mbr[510] = 0x55; card.mbr[511] = 0xaa; card.mbr[450] = 6;
    put32(card.mbr + 454, 2048); put32(card.mbr + 458, 8192);
    card.boot[0] = 0xeb; card.boot[2] = 0x90; put16(card.boot + 11, 512);
    card.boot[13] = 1; put16(card.boot + 14, 1); card.boot[16] = 2;
    put16(card.boot + 17, 512); put16(card.boot + 19, 8192); put16(card.boot + 22, 32);
    card.boot[510] = 0x55; card.boot[511] = 0xaa;
    for (i = 0; i < (full ? 16U : 1U); ++i) {
        unsigned char *entry = card.root + i * 32;
        memcpy(entry, "CFTEST  TXT", 11); entry[11] = 0x20;
        put16(entry + 26, 2); put32(entry + 28, 123);
    }
}
static int fault_active(void) { return card.commands == card.fault_command; }
static unsigned char read8(void *context, unsigned int offset)
{
    unsigned int fault;
    assert(context == &card && card.space == 0 && card.width == 8);
    if (!card.commands) {
        switch (offset) {
            case 7: case 0x0e: return 0x50;
            case 2: case 3: return 1;
            case 4: case 5: return 0;
            case 6: return 0xa0;
            default: assert(0);
        }
    }
    fault = fault_active() ? card.fault : OK;
    if (offset == 1) return 4;
    if (offset == 7) return fault == FINAL_ERROR ? 0x51 : 0x50;
    assert(offset == 0x0e);
    ++card.alternate;
    if (card.position < 256) {
        if (fault == FLOATING) return 0xff;
        if (fault == DRQ_TIMEOUT) return 0x80;
        if (fault == DRQ_ERROR) return 0x51;
        if (fault == DRQ_FAULT) return 0x70;
        if (card.delay && card.alternate < 10) return 0x81; /* ERR ignored while BSY. */
        return 0x58;
    }
    return fault == COMPLETE_TIMEOUT ? 0x80 : (fault == COMPLETE_ERROR ? 0x51 : 0x50);
}
static unsigned short read16(void *context, unsigned int offset)
{
    const unsigned char *data;
    unsigned int at;
    assert(context == &card && card.space == 0 && card.width == 16);
    if (!card.commands) { assert(offset == 6); return card.fault == BAD_MAP ? 0xa050 : 0x50a0; }
    assert(offset == 0 && card.position < 256);
    if (card.command == 0xec) data = card.id;
    else if (card.lba == 0) data = card.mbr;
    else if (card.lba == 2048) { assert(card.fat); data = card.boot; }
    else { assert(card.fat && card.lba >= 2113 && card.lba < 2117); data = card.root; }
    at = card.position++ * 2; ++card.words;
    return (unsigned int)data[at] | ((unsigned int)data[at + 1] << 8);
}
static void set_access(void *context, unsigned int space, unsigned int width)
{
    assert(context == &card && space == 0 && (width == 8 || width == 16));
    card.space = space; card.width = width;
}
static void wait_ms(void *context, unsigned int ms)
{
    assert(context == &card && ms == 1); ++card.waits;
    assert(card.waits <= PCDIAG_ATA_POLL_LIMIT_MS + 5);
}
static int issue(void *context, unsigned int command, unsigned int lba)
{
    assert(context == &card && card.space == 0 && card.width == 8);
    assert((command == 0xec && card.commands == 0 && lba == 0) ||
           (command == 0x20 && card.commands > 0 && lba < 12288));
    assert(card.commands == 0 || card.position == 256);
    ++card.commands;
    if (fault_active() && card.fault == REJECT_COMMAND) return -1;
    if (command == 0x20) ++card.reads;
    card.command = command; card.lba = lba; card.position = card.alternate = 0;
    return 0;
}
static int observe(void)
{
    static const pcdiag_cf_ops_t memory = {read8, read16, set_access, wait_ms};
    static const pcdiag_ata_ops_t ops = {&memory, issue};
    return pcdiag_ata_observe(&ops, &card, trace);
}
int main(void)
{
    unsigned int f, sum, i;
    reset(); card.delay = 1;
    assert(observe() == 0 && card.commands == 2 && card.reads == 1 && card.words == 512 && card.waits == 10);
    assert(strstr(log_buffer, "model=TRANSCEND TS4GCF133 serial=SERIAL123 firmware=V1.0"));
    assert(strstr(log_buffer, "LBA_sectors=00003000") && strstr(log_buffer, "SECTOR0 READ OK"));
    reset(); card.fault = BAD_MAP;
    assert(observe() == 1 && card.commands == 0 && card.words == 0);
    for (f = REJECT_COMMAND; f <= FINAL_ERROR; ++f) {
        reset(); card.fault = f;
        assert(observe() < 0 && card.commands == 1 && card.reads == 0);
        assert(card.words == (f >= COMPLETE_TIMEOUT ? 256U : 0U));
        if (f == DRQ_TIMEOUT || f == COMPLETE_TIMEOUT) assert(card.waits == PCDIAG_ATA_POLL_LIMIT_MS);
    }
    reset(); card.fault = DRQ_ERROR; card.fault_command = 2;
    assert(observe() < 0 && card.commands == 2 && card.reads == 1 && card.words == 256);
    reset(); put16(card.id + 98, 0); assert(observe() < 0 && card.commands == 1);
    reset(); put32(card.id + 120, 0); assert(observe() < 0 && card.commands == 1);
    reset(); put16(card.id, 0xffff); assert(observe() < 0 && card.commands == 1);
    reset(); put16(card.id + 212, 0x5000); put16(card.id + 234, 512);
    assert(observe() < 0 && card.commands == 1 && strstr(log_buffer, "logical sector size"));
    reset(); card.id[510] = 0xa5; sum = 0;
    for (i = 0; i < 511; ++i) sum += card.id[i];
    card.id[511] = (0U - sum) & 0xff;
    assert(observe() == 0 && card.reads == 1);
    reset(); card.id[510] = 0xa5; card.id[511] = 0;
    assert(observe() < 0 && card.commands == 1);
    reset(); make_fat(0);
    assert(observe() == 0 && card.commands == 4 && card.reads == 3 && card.words == 1024);
    assert(strstr(log_buffer, "type=FAT16") && strstr(log_buffer, "alias=CFTEST.TXT kind=file attr=20 bytes=123"));
    reset(); make_fat(1);
    assert(observe() == 0 && card.reads == PCDIAG_ATA_READ_LIMIT && card.commands == 7 && card.words == 1792);
    assert(strstr(log_buffer, "reason=sector-limit entries=64"));
    /* Hardware host/probe/CIS also log into this buffer: leave >12 KiB room. */
    assert(log_used < 20000);
    printf("PASS: ATA PIO command/width/order checks, 256-word transfers, bounded timeouts, errors, IDENTIFY validation and FAT listing; maximum-path ATA/FAT log=%u bytes\n", log_used);
    return 0;
}
