#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../iop/pcdev9/pcdiag_fat.h"

static unsigned char boot[512], sector0[512], root[512];
static unsigned int card_sectors, base, calls, fail_call, lbas[8];
static pcdiag_fat_geometry_t expected;
static char log_buffer[20000];
static unsigned int log_used;
static void put16(unsigned char *p, unsigned int n) { p[0] = n; p[1] = n >> 8; }
static void put32(unsigned char *p, unsigned int n) { put16(p, n); put16(p + 2, n >> 16); }
static void trace(const char *format, ...)
{
    va_list ap;
    int count;
    va_start(ap, format);
    count = vsnprintf(log_buffer + log_used, sizeof(log_buffer) - log_used, format, ap);
    va_end(ap);
    assert(count >= 0 && log_used + (unsigned int)count + 1 < sizeof(log_buffer));
    log_used += count; log_buffer[log_used++] = '\n'; log_buffer[log_used] = 0;
}
static void make_boot(unsigned int type)
{
    memset(boot, 0, sizeof(boot)); memset(sector0, 0, sizeof(sector0)); memset(root, 0, sizeof(root));
    calls = fail_call = log_used = base = 0; log_buffer[0] = 0; card_sectors = 700000;
    boot[0] = 0xeb; boot[2] = 0x90; put16(boot + 11, 512); boot[16] = 2;
    boot[510] = 0x55; boot[511] = 0xaa;
    if (type == 32) {
        boot[13] = 8; put16(boot + 14, 32); put32(boot + 32, 600000);
        put32(boot + 36, 600); put32(boot + 44, 5);
    } else {
        boot[13] = 1; put16(boot + 14, 1);
        put16(boot + 17, type == 12 ? 224 : 512);
        put16(boot + 19, type == 12 ? 2880 : 8192); put16(boot + 22, type == 12 ? 9 : 32);
    }
}
static int read_sector(void *context, unsigned int lba, unsigned char *data)
{
    assert(context == &calls && calls < 5 && lba < card_sectors);
    lbas[calls++] = lba;
    if (calls == fail_call) return -1;
    if (base && lba == base) memcpy(data, boot, 512);
    else {
        assert(lba >= expected.root_lba && lba < expected.root_lba + expected.root_sectors);
        memcpy(data, root, 512);
    }
    return 0;
}
static int preview(void) { return pcdiag_fat_preview(sector0, card_sectors, read_sector, &calls, trace); }
static void partition(unsigned int slot, unsigned int type, unsigned int start, unsigned int length)
{
    unsigned char *entry = sector0 + 446 + slot * 16;
    sector0[510] = 0x55; sector0[511] = 0xaa; entry[4] = type;
    put32(entry + 8, start); put32(entry + 12, length);
}
int main(void)
{
    unsigned int type, i, j, seed = 17;
    unsigned char good[512];
    for (type = 12; type <= 32; type += type == 12 ? 4 : 16) {
        make_boot(type);
        assert(pcdiag_fat_geometry(boot, 0, card_sectors, &expected) == 0 && expected.type == type);
        assert(expected.root_lba == (type == 12 ? 19U : (type == 16 ? 65U : 1256U)));
        memcpy(sector0, boot, 512);
        root[0] = 0xe5; /* deleted */
        root[32] = 0x41; root[43] = 0x0f; /* LFN */
        memcpy(root + 64, "CFTEST  TXT", 11); root[75] = 0x20;
        put16(root + 90, 2); put32(root + 92, 1234);
        memcpy(root + 96, "SUBDIR     ", 11); root[107] = 0x10;
        memcpy(root + 128, "LABEL      ", 11); root[139] = 8;
        assert(preview() == 0 && calls == 1 && lbas[0] == expected.root_lba);
        assert(strstr(log_buffer, "alias=CFTEST.TXT kind=file attr=20 bytes=1234"));
        assert(strstr(log_buffer, "alias=SUBDIR kind=directory") && strstr(log_buffer, "alias=LABEL kind=label"));
        assert(strstr(log_buffer, "entries=3 LFN_records_skipped=1"));
    }
    make_boot(32); base = 2048;
    assert(pcdiag_fat_geometry(boot, base, 600000, &expected) == 0);
    partition(0, 0x0c, 0xfffffff0U, 600000); /* overflow/outside: skip */
    partition(1, 0x0c, base, 600000);
    for (i = 0; i < 16; ++i) { memcpy(root + i * 32, "FILE    BIN", 11); root[i * 32 + 11] = 0x20; }
    assert(preview() == 0 && calls == 5 && lbas[0] == base && lbas[4] == expected.root_lba + 3);
    assert(strstr(log_buffer, "reason=sector-limit entries=64"));
    make_boot(16); base = 2048; partition(0, 6, base, 8192);
    assert(pcdiag_fat_geometry(boot, base, 8192, &expected) == 0);
    fail_call = 2; assert(preview() < 0 && calls == 2); /* root read fails */
    make_boot(16); partition(0, 6, card_sectors - 1, 8192);
    assert(preview() == 1 && calls == 0); /* partition extends past card */
    make_boot(16); partition(0, 0xee, 1, 600000);
    assert(preview() == 1 && calls == 0); /* GPT not followed */
    make_boot(16); base = 2048; partition(0, 6, base, 8192);
    put16(boot + 11, 4096); assert(preview() == 1 && calls == 1);
    make_boot(16); memcpy(good, boot, 512);
    for (i = 0; i < 8; ++i) {
        memcpy(boot, good, 512);
        switch (i) {
            case 0: boot[510] = 0; break;
            case 1: put16(boot + 11, 4096); break;
            case 2: boot[13] = 3; break;
            case 3: put16(boot + 14, 0); break;
            case 4: boot[16] = 255; break;
            case 5: put16(boot + 22, 1); break; /* FAT too small */
            case 6: put16(boot + 17, 0); break;
            case 7: put16(boot + 19, 65535); break;
        }
        assert(pcdiag_fat_geometry(boot, 0, 8192, &expected) == 1);
    }
    make_boot(32); put32(boot + 44, 0xffffffffU);
    assert(pcdiag_fat_geometry(boot, 0, card_sectors, &expected) == 1);
    make_boot(32); put16(boot + 42, 1);
    assert(pcdiag_fat_geometry(boot, 0, card_sectors, &expected) == 1);
    make_boot(12);
    assert(pcdiag_fat_geometry(boot, 0x0fffffffU, 2, &expected) == 1);
    /* Corrupt valid BPBs, including overflow candidates. Accepted layouts
       must still stay entirely inside both volume extent and LBA28. */
    for (i = 0; i < 500; ++i) {
        make_boot(i % 3 == 0 ? 12 : (i % 3 == 1 ? 16 : 32));
        for (j = 0; j < 5; ++j) {
            seed = seed * 1664525U + 1013904223U;
            boot[11 + seed % 42] = seed >> 24;
        }
        if (pcdiag_fat_geometry(boot, 2048, 600000, &expected) == 0) {
            assert(expected.type == 12 || expected.type == 16 || expected.type == 32);
            assert(expected.root_lba >= 2048 && expected.root_lba < 2048 + expected.total_sectors);
            assert(expected.root_sectors <= 2048 + expected.total_sectors - expected.root_lba);
            assert(expected.total_sectors <= 600000);
        }
    }
    puts("PASS: FAT12/16/32 geometry, superfloppy/primary partitions, deleted/LFN/8.3 entries, read errors, four-sector root limit and 500 corrupted BPBs");
    return 0;
}
