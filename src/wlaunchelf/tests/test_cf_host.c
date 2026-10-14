/* Compile the exact hardware-wrapper block from production ps2dev9.c
   against mock MMIO, not a parallel reimplementation of the wrapper. */
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../iop/pcdev9/pcdiag_ata.h"
typedef uint16_t u16;
typedef uint32_t u32;
static u16 selector, power, mode, cardstat;
static u32 timing, original_timing;
static unsigned int writes, waits, card_reads;
static unsigned int ata_calls, byte_accesses;
static unsigned char taskfile[16];
static unsigned char cor;
#define DEV9_R_1460 0x1460
#define DEV9_R_1462 0x1462
#define DEV9_R_POWER 0x146c
#define DEV9_R_1474 0x1474
#define SSBUS_R_1418 0xbf801418U
#define USE_DEV9_REGS
#define USE_SPD_REGS
static u16 *controller_register(unsigned int offset)
{
    switch (offset) {
        case DEV9_R_1460: return &selector;
        case DEV9_R_1462: return &cardstat;
        case DEV9_R_POWER: return &power;
        case DEV9_R_1474: return &mode;
        default: assert(!"unexpected controller register"); return NULL;
    }
}
#define DEV9_REG(offset) (*controller_register(offset))
static u32 read_timing(unsigned int address)
{
    assert(address == SSBUS_R_1418);
    return timing;
}
static void write_timing(u32 value, unsigned int address)
{
    assert(address == SSBUS_R_1418);
    assert((value & ~0x1000U) == (original_timing & ~0x1000U));
    timing = value;
    ++writes;
}
#define _lw(address) read_timing(address)
#define _sw(value,address) write_timing(value,address)
static unsigned short card_word(unsigned int offset)
{
    assert(timing & 0x1000U);
    assert((selector & 4) == 4);
    assert((selector & 3) == 0 || (selector & 3) == 3);
    assert(offset == 2 || offset == 4 || offset == 6 || offset == 0x0c || offset == 0x0e);
    ++card_reads;
    return offset == 6 ? 0x5000 : 0x0101;
}
/* Word data register remains read-only in the production wrapper. */
#define SPD_REG16(offset) card_word(offset)
static unsigned char *taskfile_register(unsigned int offset)
{
    assert(!(timing & 0x1000U) && (selector & 3) == 0);
    assert(offset == 2 || offset == 3 || offset == 4 || offset == 5 ||
           offset == 6 || offset == 7 || offset == 0x0e);
    ++byte_accesses;
    return taskfile + offset;
}
#define SPD_REG8(offset) (*taskfile_register(offset))
static unsigned char pcdiag_attribute_read(void *context, unsigned int offset)
{
    (void)context;
    assert(!(timing & 0x1000U));
    assert((selector & 4) == 4);
    ++card_reads;
    if ((selector & 3) == 2) {
        assert(offset >= 0x200 && offset <= 0x206 && !(offset & 1));
        return offset == 0x200 ? cor : 0;
    }
    assert((selector & 3) == 0 || (selector & 3) == 3);
    assert((offset >= 1 && offset <= 7) || (offset >= 0x0d && offset <= 0x0f));
    return offset == 7 || offset == 0x0e ? 0x50 : 1;
}
static void pccardTrace(const char *format, ...)
{
    va_list ap;
    char line[512];
    int count;
    va_start(ap, format);
    count = vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    assert(count >= 0 && count < (int)sizeof(line));
}
#define M_PRINTF(...) pccardTrace(__VA_ARGS__)
static void delay_thread(unsigned int microseconds)
{
    assert(microseconds == (waits == 0 ? 500000U : 100000U));
    assert(waits < 3);
    ++waits;
}
#define DelayThread(microseconds) delay_thread(microseconds)
/* Exercise actual command writer and restoration after a failed observation.
   Protocol state machine itself is tested separately, without this stub. */
int pcdiag_ata_observe(const pcdiag_ata_ops_t *ops, void *context, pcdiag_cis_trace_t trace)
{
    unsigned int before;
    (void)trace;
    ++ata_calls;
    ops->memory->set_access(context, 0, 8);
    before = byte_accesses;
    assert(ops->issue_read_command(context, 0x30, 0) < 0);
    assert(ops->issue_read_command(context, 0xef, 0) < 0);
    assert(ops->issue_read_command(context, 0xec, 1) < 0);
    assert(ops->issue_read_command(context, 0x20, 0x10000000U) < 0);
    assert(byte_accesses == before);
    assert(ops->issue_read_command(context, 0xec, 0) == 0);
    assert(byte_accesses == before + 1 && taskfile[7] == 0xec);
    assert(ops->issue_read_command(context, 0x20, 0x0abcdef1U) == 0);
    assert(byte_accesses == before + 11 && taskfile[7] == 0x20);
    assert(taskfile[6] == 0xea && taskfile[2] == 1 && taskfile[3] == 0xf1 &&
           taskfile[4] == 0xde && taskfile[5] == 0xbc);
    ops->memory->set_access(context, 3, 8);
    before = byte_accesses;
    assert(ops->issue_read_command(context, 0xec, 0) < 0);
    assert(byte_accesses == before);
    ops->memory->set_access(context, 0, 16);
    assert(ops->issue_read_command(context, 0x20, 0) < 0);
    assert(byte_accesses == before);
    return -3; /* Simulated timeout: exact host restoration must still run. */
}
#include "actual_cf_host.h"
static void reset(unsigned char value)
{
    selector = 6; /* Attribute space, with unknown bit 2 set. */
    power = 0x17; mode = 0; cardstat = 0x1b8;
    timing = original_timing = 0x001a00bbU;
    cor = value;
    writes = waits = card_reads = 0;
    ata_calls = byte_accesses = 0;
    memset(taskfile, 0, sizeof(taskfile));
}
int main(void)
{
    pcdiag_cis_info_t info = {1,0,0,1,4,1,0x200,15,1};
    reset(0);
    pcdiag_cf_host_probe(&info);
    assert(writes == 16 && waits == 3 && card_reads == 94 && ata_calls == 1);
    assert(selector == 6 && timing == original_timing);
    assert(power == 0x17 && mode == 0 && cardstat == 0x1b8);
    reset(2); /* COR refuses probing, but restoration still runs. */
    pcdiag_cf_host_probe(&info);
    assert(writes == 1 && waits == 0 && card_reads == 4 && ata_calls == 0 && byte_accesses == 0);
    assert(selector == 6 && timing == original_timing && power == 0x17 && mode == 0);
    reset(0); info.function = 0xfe; /* e.g. Sony card: no MMIO at all. */
    pcdiag_cf_host_probe(&info);
    assert(writes == 0 && waits == 0 && card_reads == 0 && ata_calls == 0 && byte_accesses == 0);
    puts("PASS: production CF wrapper restores host registers after timeout, preserves power/voltage and unknown bits; actual writer accepts only EC/20 in correct byte window");
    return 0;
}
