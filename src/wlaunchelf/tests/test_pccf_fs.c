/* Actual IOMANX module + FatFs on sparse FAT images. No parallel parser.
   Driver is exercised through its registered operation table. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#define _start cf_module_start
#include "../iop/pccf/pccf_fs.c"
#undef _start

#define MAP_SECTORS 128
typedef struct { unsigned int lba; unsigned char data[512]; } mapped_t;
static mapped_t map[MAP_SECTORS];
static unsigned int mappings, capacity, volume_base, fat_base, root_base, data_base, spc, fat_type;
static unsigned int io_reads, stops, present, fail_sector, add_failure, sema_count, locked;
static iop_device_t *registered;
static char log_text[200000];
static unsigned int log_len;
void pccardTrace(const char *format, ...)
{
    va_list ap; int n;
    va_start(ap, format); n = vsnprintf(log_text + log_len, sizeof(log_text) - log_len, format, ap); va_end(ap);
    assert(n >= 0 && log_len + (unsigned int)n + 1 < sizeof(log_text));
    log_len += n; log_text[log_len++] = '\n'; log_text[log_len] = 0;
}
int CreateSema(iop_sema_t *s) { assert(!sema_count && s->initial == 1 && s->max == 1); sema_count = 1; return 1; }
int DeleteSema(int id) { assert(id == 1 && sema_count && !locked); sema_count = 0; return 0; }
int WaitSema(int id) { assert(id == 1 && sema_count && !locked); locked = 1; return 0; }
int SignalSema(int id) { assert(id == 1 && sema_count && locked); locked = 0; return 0; }
int AddDrv(iop_device_t *d)
{
    assert(!strcmp(d->name, "cf") && (d->type & IOP_DT_FSEXT));
    if (add_failure) return -1;
    registered = d; return d->ops->init(d);
}
int pccf_hw_present(void) { return present; }
void pccf_hw_stop(void) { ++stops; present = 0; }
int pccf_hw_start(pccf_ata_t *a) { memset(a, 0, sizeof(*a)); a->ready = present = 1; a->sectors = capacity; return 0; }
static unsigned char *mapped(unsigned int lba)
{
    unsigned int i;
    assert(lba < capacity);
    for (i = 0; i < mappings; ++i) if (map[i].lba == lba) return map[i].data;
    assert(mappings < MAP_SECTORS); i = mappings++;
    map[i].lba = lba; memset(map[i].data, 0, 512); return map[i].data;
}
int pccf_ata_read(pccf_ata_t *a, unsigned int lba, unsigned int count, unsigned char *buffer)
{
    unsigned int i, j;
    assert(a->ready && present && count && count <= 64 && lba < capacity && count <= capacity - lba);
    for (i = 0; i < count; ++i) {
        ++io_reads;
        if (lba + i == fail_sector) { a->ready = 0; return -1; }
        memset(buffer + i * 512, 0, 512);
        for (j = 0; j < mappings; ++j)
            if (map[j].lba == lba + i) memcpy(buffer + i * 512, map[j].data, 512);
        ++a->sector_reads;
    }
    return 0;
}
static void put16(unsigned char *p, unsigned int v) { p[0] = v; p[1] = v >> 8; }
static void put32(unsigned char *p, unsigned int v) { put16(p, v); put16(p + 2, v >> 16); }
static void fat_entry(unsigned int cluster, unsigned int value)
{
    unsigned int offset, old;
    unsigned char *p;
    offset = fat_type == 12 ? cluster * 3 / 2 : cluster * (fat_type == 16 ? 2 : 4);
    p = mapped(fat_base + offset / 512) + offset % 512;
    assert(offset % 512 < 508);
    if (fat_type == 12) {
        old = p[0] | ((unsigned int)p[1] << 8);
        put16(p, cluster & 1 ? ((old & 15) | (value << 4)) : ((old & 0xf000) | value));
    } else if (fat_type == 16) put16(p, value);
    else put32(p, value);
}
static unsigned int cluster_lba(unsigned int c) { return data_base + (c - 2) * spc; }
static unsigned char *root_entry(unsigned int index)
{
    unsigned int lba = root_base + index / 16;
    if (fat_type == 32 && index >= spc * 16) lba = cluster_lba(20) + (index - spc * 16) / 16;
    return mapped(lba) + (index % 16) * 32;
}
static void short_entry(unsigned char *p, const char *alias, unsigned int attr, unsigned int cluster, unsigned int size)
{
    memset(p, 0, 32); memcpy(p, alias, 11); p[11] = attr;
    put16(p + 26, cluster); if (fat_type == 32) put16(p + 20, cluster >> 16);
    put32(p + 28, size);
    put16(p + 16, ((2026 - 1980) << 9) | (10 << 5) | 1); put16(p + 14, (1 << 11) | (26 << 5) | 14);
    put16(p + 24, ((2026 - 1980) << 9) | (10 << 5) | 1); put16(p + 22, (1 << 11) | (27 << 5) | 15);
}
static unsigned int alias_checksum(const char *alias)
{
    unsigned int i, s = 0;
    for (i = 0; i < 11; ++i) s = ((s & 1) << 7) + (s >> 1) + (unsigned char)alias[i], s &= 255;
    return s;
}
static unsigned int lfn_entries(unsigned int index, const unsigned short *name, unsigned int size, const char *alias)
{
    static const unsigned int offsets[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
    unsigned int n = (size + 12) / 13, j, k, at;
    unsigned char *p;
    for (j = n; j > 0; --j) {
        p = root_entry(index++); memset(p, 0xff, 32); p[0] = j | (j == n ? 0x40 : 0);
        p[11] = 0x0f; p[12] = 0; p[13] = alias_checksum(alias); put16(p + 26, 0);
        for (k = 0; k < 13; ++k) { at = (j - 1) * 13 + k; put16(p + offsets[k], at < size ? name[at] : (at == size ? 0 : 0xffff)); }
    }
    return index;
}
static unsigned char pattern(unsigned int at) { return (at * 17 + fat_type) & 255; }
static void make_image(unsigned int type, int partitioned)
{
    unsigned char *b, *p; unsigned int total, reserved, fsize, root_sectors, i, j, index = 0, eoc;
    static const unsigned short long_name[] = {'L','o','n','g',' ','f','i','l','e','n','a','m','e','.','t','x','t'};
    static const unsigned short russian[] = {0x422,0x435,0x441,0x442,'.','t','x','t'};
    assert(!sema_count && !locked);
    mappings = io_reads = stops = add_failure = log_len = 0; log_text[0] = 0; registered = NULL;
    fail_sector = 0xffffffffU; fat_type = type;
    total = type == 12 ? 2880 : (type == 16 ? 8192 : 600000);
    volume_base = partitioned ? 63 : 0; capacity = volume_base + total + 100;
    spc = type == 32 ? 8 : 1; reserved = type == 32 ? 32 : 1;
    fsize = type == 12 ? 9 : (type == 16 ? 32 : 600);
    root_sectors = type == 12 ? 14 : (type == 16 ? 32 : 0);
    fat_base = volume_base + reserved; data_base = fat_base + 2 * fsize + root_sectors;
    root_base = type == 32 ? data_base : fat_base + 2 * fsize;
    b = mapped(volume_base); b[0] = 0xeb; b[1] = 0x58; b[2] = 0x90; memcpy(b + 3, "MSDOS5.0", 8);
    put16(b + 11, 512); b[13] = spc; put16(b + 14, reserved); b[16] = 2; b[21] = 0xf8;
    if (type == 32) {
        put32(b + 32, total); put32(b + 36, fsize); put32(b + 44, 2);
        put16(b + 48, 1); put16(b + 50, 6); b[66] = 0x29; memcpy(b + 82, "FAT32   ", 8);
    } else {
        put16(b + 17, root_sectors * 16); put16(b + 19, total); put16(b + 22, fsize);
        b[38] = 0x29; memcpy(b + 54, type == 12 ? "FAT12   " : "FAT16   ", 8);
    }
    b[510] = 0x55; b[511] = 0xaa;
    if (partitioned) {
        p = mapped(0); p[510] = 0x55; p[511] = 0xaa; p[446] = 0x80;
        p[450] = type == 32 ? 0x0c : 6; put32(p + 454, volume_base); put32(p + 458, total);
    }
    eoc = type == 12 ? 0xfff : (type == 16 ? 0xffff : 0x0fffffff);
    fat_entry(0, eoc & ~7U); fat_entry(1, eoc); fat_entry(3, eoc);
    fat_entry(5, 9); fat_entry(9, eoc); fat_entry(6, eoc); fat_entry(7, eoc); fat_entry(8, eoc); fat_entry(11, eoc);
    if (type == 32) { fat_entry(2, 20); fat_entry(20, eoc); }
    index = lfn_entries(index, long_name, 17, "LONGFI~1TXT");
    short_entry(root_entry(index++), "LONGFI~1TXT", 0x20, 5, spc * 512 + 333);
    short_entry(root_entry(index++), "SUBDIR     ", 0x10, 3, 0);
    short_entry(root_entry(index++), "EMPTY   TXT", 0x20, 0, 0);
    p = root_entry(index++); short_entry(p, "LOWER   TXT", 0x20, 7, 3); p[12] = 0x18;
    index = lfn_entries(index, russian, 8, "RUSSI~1 TXT");
    short_entry(root_entry(index++), "RUSSI~1 TXT", 0x20, 11, 3);
    while (index < 128) {
        char alias[12]; snprintf(alias, sizeof(alias), "F%07uTXT", index);
        short_entry(root_entry(index++), alias, 0x20, 0, 0);
    }
    short_entry(root_entry(index), "AFTER   TXT", 0x20, 8, 5);
    p = mapped(cluster_lba(3)); short_entry(p, ".          ", 0x10, 3, 0);
    short_entry(p + 32, "..         ", 0x10, type == 32 ? 2 : 0, 0);
    short_entry(p + 64, "INNER   TXT", 0x20, 6, 4);
    for (i = 0; i < spc; ++i) { p = mapped(cluster_lba(5) + i); for (j = 0; j < 512; ++j) p[j] = pattern(i * 512 + j); }
    p = mapped(cluster_lba(9)); for (j = 0; j < 333; ++j) p[j] = pattern(spc * 512 + j);
    memcpy(mapped(cluster_lba(6)), "CFOK", 4); memcpy(mapped(cluster_lba(7)), "low", 3);
    memcpy(mapped(cluster_lba(8)), "after", 5); memcpy(mapped(cluster_lba(11)), "rus", 3);
}
static const char rus_name[] = "/\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82.txt";
static void test_volume(unsigned int type, int partitioned)
{
    iop_file_t dir = {0}, file = {0}, other = {0}, slots[CF_HANDLES + 1] = {{0}};
    iox_dirent_t entry; iox_stat_t stat; unsigned char data[5000];
    unsigned int count = 0, i, found_long = 0, found_after = 0, found_rus = 0, size;
    pccf_info_t status;
    make_image(type, partitioned);
    assert(cf_module_start(0, NULL) == 0 && registered == &device && mounted && present);
    assert(geometry.type == type && geometry.volume_base == volume_base);
    assert(registered->ops->dopen(&dir, "/") == 0);
    assert(registered->ops->deinit(registered) == -EBUSY && present);
    while (registered->ops->dread(&dir, &entry) > 0) {
        ++count; assert(!(entry.stat.mode & (FIO_S_IWUSR | FIO_S_IWGRP | FIO_S_IWOTH)));
        if (!strcmp(entry.name, "Long filename.txt")) { found_long = 1; assert(entry.stat.size == spc * 512 + 333); }
        if (!strcmp(entry.name, "AFTER.TXT")) found_after = 1;
        if (!strcmp(entry.name, rus_name + 1)) found_rus = 1;
    }
    assert(count > 64 && found_long && found_after && found_rus);
    assert(registered->ops->dclose(&dir) == 0);
    assert(registered->ops->getstat(&file, "/", &stat) == 0 && FIO_S_ISDIR(stat.mode));
    assert(registered->ops->getstat(&file, "/Long filename.txt", &stat) == 0 && FIO_S_ISREG(stat.mode));
    assert(stat.mtime[6] == (2026 & 255) && stat.mtime[7] == (2026 >> 8) && stat.mtime[5] == 10 && stat.mtime[4] == 1);
    assert(registered->ops->open(&file, "/Long filename.txt", O_RDONLY, 0) == 0);
    size = spc * 512 + 333;
    assert(registered->ops->read(&file, data, sizeof(data)) == (int)size);
    for (i = 0; i < size; ++i) assert(data[i] == pattern(i));
    assert(registered->ops->read(&file, data, 1) == 0);
    assert(registered->ops->lseek(&file, spc * 512 - 100, SEEK_SET) == (int)spc * 512 - 100);
    assert(registered->ops->read(&file, data, 200) == 200);
    for (i = 0; i < 200; ++i) assert(data[i] == pattern(spc * 512 - 100 + i));
    assert(registered->ops->lseek64(&file, -4, SEEK_END) == size - 4);
    assert(registered->ops->read(&file, data, 8) == 4);
    assert(registered->ops->lseek64(&file, 0x7fffffffffffffffLL, SEEK_CUR) == -EINVAL);
    assert(registered->ops->lseek(&file, -1, SEEK_SET) == -EINVAL);
    assert(registered->ops->open(&other, "/SUBDIR/INNER.TXT", O_RDONLY, 0) == 0);
    assert(registered->ops->read(&other, data, 20) == 4 && !memcmp(data, "CFOK", 4));
    assert(registered->ops->close(&other) == 0);
    assert(registered->ops->open(&other, rus_name, O_RDONLY, 0) == 0);
    assert(registered->ops->read(&other, data, 20) == 3 && !memcmp(data, "rus", 3));
    assert(registered->ops->close(&other) == 0);
    assert(registered->ops->open(&other, "/lower.txt", O_RDONLY, 0) == 0);
    assert(registered->ops->close(&other) == 0);
    assert(registered->ops->open(&other, "/EMPTY.TXT", O_RDONLY, 0) == 0);
    assert(registered->ops->read(&other, data, 20) == 0 && registered->ops->close(&other) == 0);
    assert(registered->ops->close(&file) == 0 && registered->ops->close(&file) == -EBADF);
    for (i = 0; i < CF_HANDLES; ++i) assert(registered->ops->open(slots + i, "/EMPTY.TXT", O_RDONLY, 0) == 0);
    assert(registered->ops->dopen(slots + CF_HANDLES, "/") == -EMFILE);
    for (i = 0; i < CF_HANDLES; ++i) assert(registered->ops->close(slots + i) == 0);
    i = io_reads;
    assert(registered->ops->open(&file, "/NEW.TXT", O_WRONLY | O_CREAT, 0) == -EROFS);
    assert(registered->ops->open(&file, "/EMPTY.TXT", O_RDONLY | O_TRUNC, 0) == -EROFS);
    assert(registered->ops->write(&file, data, 1) == -EROFS);
    assert(registered->ops->remove(&file, "/EMPTY.TXT") == -EROFS);
    assert(registered->ops->mkdir(&file, "/DIR", 0) == -EROFS);
    assert(registered->ops->rename(&file, "/EMPTY.TXT", "/X.TXT") == -EROFS);
    assert(registered->ops->format(&file, NULL, NULL, NULL, 0) == -EROFS);
    assert(disk_write(0, data, 0, 1) == RES_WRPRT && io_reads == i);
    assert(registered->ops->open(&file, "1:/EMPTY.TXT", O_RDONLY, 0) == -EINVAL);
    other.unit = 1; assert(registered->ops->dopen(&other, "/") == -ENODEV); other.unit = 0;
    assert(registered->ops->devctl(&file, "", PCCF_DEVCTL_INFO, NULL, 0, &status, sizeof(status)) == 0 && status.online && status.fat_type == type);
    assert(registered->ops->deinit(registered) == 0 && !present && !sema_count && stops == 1);
    printf("PASS: FAT%u%s, long/UTF-8 names, >64 root entries%s, nested file, fragmented read/seek/EOF, handle limit, timestamps and all write rejections\n", type, partitioned ? " MBR" : " superfloppy", type == 32 ? "/linked root clusters" : "");
}
int main(void)
{
    iop_file_t file = {0}; unsigned char buffer[5000]; unsigned int before; int result;
    test_volume(12, 0); test_volume(16, 1); test_volume(32, 1);
    make_image(32, 1); assert(cf_module_start(0, NULL) == 0);
    assert(device.ops->open(&file, "/Long filename.txt", O_RDONLY, 0) == 0);
    fail_sector = cluster_lba(9);
    assert(device.ops->read(&file, buffer, sizeof(buffer)) == (int)spc * 512); /* Partial bytes before failing second cluster. */
    before = io_reads; assert(device.ops->read(&file, buffer, 1) == -ENODEV && io_reads == before);
    assert(device.ops->close(&file) == 0 && device.ops->deinit(&device) == 0);
    make_image(32, 1); assert(cf_module_start(0, NULL) == 0);
    assert(device.ops->open(&file, "/EMPTY.TXT", O_RDONLY, 0) == 0);
    present = 0; before = io_reads;
    assert(device.ops->read(&file, buffer, 1) == -ENODEV && io_reads == before);
    assert(device.ops->close(&file) == 0 && device.ops->deinit(&device) == 0);
    make_image(32, 1); fat_entry(2, 2); assert(cf_module_start(0, NULL) == 0);
    before = io_reads; result = device.ops->open(&file, "/MISSING.TXT", O_RDONLY, 0);
    /* FatFs' own maximum-directory bound can terminate before our budget. */
    assert((result == -EIO || result == -ENOENT) && io_reads - before <= CF_IO_SECTOR_BUDGET);
    assert(lock() == 0); budget = 0;
    assert(disk_read(0, buffer, root_base, 1) == RES_ERROR);
    assert(strstr(log_text, "sector budget exhausted")); unlock(0);
    assert(device.ops->deinit(&device) == 0);
    make_image(16, 1); add_failure = 1;
    assert(cf_module_start(0, NULL) == 1 && !present && !sema_count && !mounted && stops == 1);
    make_image(16, 1); put32(mapped(0) + 458, capacity); /* Out-of-range MBR extent. */
    assert(cf_module_start(0, NULL) == 1 && !registered && !present && stops == 1);
    make_image(12, 0); mapped(0)[510] = 0;
    assert(cf_module_start(0, NULL) == 1 && !registered && !present && stops == 1);
    puts("PASS: read failure partial data/offline latch, card removal, bounded cyclic root chain, registration/invalid-volume failure cleanup");
    return 0;
}
