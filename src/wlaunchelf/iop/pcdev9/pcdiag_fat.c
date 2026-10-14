/* Bounded root-directory observation, not a mounted filesystem.
   Layout checked against Microsoft FAT Specification (2005), sections 3/6.
   Partition extents and BPB values can never authorize out-of-range reads.
   Single initialization caller; no heap, FAT traversal, or recursive scans. */
#include "pcdiag_fat.h"

static unsigned char scratch_sector[512];
static unsigned int le16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}
static unsigned int le32(const unsigned char *p)
{
    return le16(p) | (le16(p + 2) << 16);
}
int pcdiag_fat_geometry(const unsigned char *boot, unsigned int base,
                         unsigned int extent, pcdiag_fat_geometry_t *geometry)
{
    unsigned int total, reserved, fats, fat_sectors, root_entries, root_sectors;
    unsigned int data_start, clusters, spc, root_cluster, required_fat, type;
    if (base >= 0x10000000U || extent == 0 || extent > 0x10000000U - base ||
        boot[510] != 0x55 || boot[511] != 0xaa || le16(boot + 11) != 512 ||
        !((boot[0] == 0xeb && boot[2] == 0x90) || boot[0] == 0xe9)) return 1;
    spc = boot[13]; reserved = le16(boot + 14); fats = boot[16];
    if (!spc || spc > 128 || (spc & (spc - 1)) || !reserved ||
        (fats != 1 && fats != 2)) return 1;
    total = le16(boot + 19);
    if (!total) total = le32(boot + 32);
    fat_sectors = le16(boot + 22);
    if (!fat_sectors) fat_sectors = le32(boot + 36);
    root_entries = le16(boot + 17);
    root_sectors = (root_entries * 32U + 511) / 512;
    if (!total || total > extent || reserved >= total || !fat_sectors ||
        fat_sectors > (total - reserved) / fats) return 1;
    data_start = reserved + fats * fat_sectors;
    if (root_sectors >= total - data_start) return 1;
    data_start += root_sectors;
    clusters = (total - data_start) / spc;
    if (!clusters || clusters > 0x0fffffedU) return 1;
    type = clusters < 4085 ? 12 : (clusters < 65525 ? 16 : 32);
    if (type == 32) {
        if (le16(boot + 22) || root_entries || le16(boot + 42)) return 1;
        root_cluster = le32(boot + 44);
        if (root_cluster < 2 || root_cluster > clusters + 1) return 1;
        root_sectors = spc;
        geometry->root_lba = base + data_start + (root_cluster - 2) * spc;
        required_fat = (clusters + 2 + 127) / 128;
    } else {
        if (!le16(boot + 22) || !root_entries || (root_entries & 15)) return 1;
        geometry->root_lba = base + reserved + fats * fat_sectors;
        required_fat = type == 16 ? (clusters + 2 + 255) / 256 : ((clusters + 2) * 3 + 1023) / 1024;
    }
    if (fat_sectors < required_fat || geometry->root_lba < base ||
        geometry->root_lba >= base + total || root_sectors > base + total - geometry->root_lba) return 1;
    geometry->type = type; geometry->volume_base = base;
    geometry->total_sectors = total; geometry->clusters = clusters;
    geometry->root_sectors = root_sectors;
    return 0;
}
static int fat_partition_type(unsigned int type)
{
    return type == 1 || type == 4 || type == 6 || type == 0x0e || type == 0x0b || type == 0x0c;
}
static void short_name(const unsigned char *entry, char *name)
{
    unsigned int i, end = 8, extension = 3, at = 0;
    while (end && entry[end - 1] == ' ') --end;
    while (extension && entry[8 + extension - 1] == ' ') --extension;
    for (i = 0; i < end; ++i) name[at++] = entry[i] >= 32 && entry[i] <= 126 ? entry[i] : '?';
    if (extension) name[at++] = '.';
    for (i = 0; i < extension; ++i) name[at++] = entry[8 + i] >= 32 && entry[8 + i] <= 126 ? entry[8 + i] : '?';
    name[at] = 0;
}
int pcdiag_fat_preview(const unsigned char *sector0, unsigned int card_sectors,
                        pcdiag_read_sector_t read_sector, void *context,
                        pcdiag_cis_trace_t trace)
{
    pcdiag_fat_geometry_t geometry;
    unsigned int i, base = 0, extent = card_sectors, chosen = 0, root_limit;
    unsigned int entry_index, sector_index, entries = 0, lfn = 0, cluster, type;
    const unsigned char *entry;
    char name[13];
    if (pcdiag_fat_geometry(sector0, 0, card_sectors, &geometry)) {
        if (sector0[510] != 0x55 || sector0[511] != 0xaa) {
            trace("dev9: CF FAT SKIP: sector0 has no 55aa signature");
            return 1;
        }
        for (i = 0; i < 4; ++i) {
            entry = sector0 + 446 + i * 16;
            type = entry[4];
            trace("dev9: CF MBR partition=%u boot=%02x type=%02x start=%08lx sectors=%08lx",
                  i + 1, (unsigned int)entry[0], type, (unsigned long)le32(entry + 8), (unsigned long)le32(entry + 12));
            if (!chosen && (entry[0] == 0 || entry[0] == 0x80) && fat_partition_type(type) &&
                le32(entry + 8) > 0 && le32(entry + 8) < card_sectors &&
                le32(entry + 12) > 0 && le32(entry + 12) <= card_sectors - le32(entry + 8)) {
                chosen = i + 1; base = le32(entry + 8); extent = le32(entry + 12);
            }
        }
        if (!chosen) {
            trace("dev9: CF FAT SKIP: no bounded supported primary FAT partition; GPT/extended/APA/NTFS/exFAT not followed");
            return 1;
        }
        if (read_sector(context, base, scratch_sector)) return -1;
        if (pcdiag_fat_geometry(scratch_sector, base, extent, &geometry)) {
            trace("dev9: CF FAT SKIP: chosen partition BPB is unsupported or inconsistent");
            return 1;
        }
    }
    trace("dev9: CF FAT VOLUME type=FAT%u base=%08lx sectors=%08lx clusters=%08lx root_lba=%08lx root_span=%u",
          geometry.type, (unsigned long)geometry.volume_base, (unsigned long)geometry.total_sectors,
          (unsigned long)geometry.clusters, (unsigned long)geometry.root_lba, geometry.root_sectors);
    root_limit = geometry.root_sectors < PCDIAG_FAT_ROOT_SECTOR_LIMIT ? geometry.root_sectors : PCDIAG_FAT_ROOT_SECTOR_LIMIT;
    trace("dev9: CF FAT ROOT BEGIN max_sectors=%u; 8.3 aliases only, no linked-cluster traversal", root_limit);
    for (sector_index = 0; sector_index < root_limit; ++sector_index) {
        if (read_sector(context, geometry.root_lba + sector_index, scratch_sector)) return -1;
        for (entry_index = 0; entry_index < 16; ++entry_index) {
            entry = scratch_sector + entry_index * 32;
            if (entry[0] == 0) {
                trace("dev9: CF FAT ROOT END reason=directory-end-marker entries=%u LFN_records_skipped=%u", entries, lfn);
                return 0;
            }
            if (entry[0] == 0xe5) continue;
            if ((entry[11] & 0x3f) == 0x0f) { ++lfn; continue; }
            short_name(entry, name);
            cluster = le16(entry + 26);
            if (geometry.type == 32) cluster |= le16(entry + 20) << 16;
            trace("dev9: CF FAT ROOT ENTRY alias=%s kind=%s attr=%02x bytes=%lu first_cluster=%08lx",
                  name, (entry[11] & 8) ? "label" : ((entry[11] & 0x10) ? "directory" : "file"),
                  (unsigned int)entry[11], (unsigned long)le32(entry + 28), (unsigned long)cluster);
            ++entries;
        }
    }
    trace("dev9: CF FAT ROOT STOP reason=%s entries=%u LFN_records_skipped=%u; listing may be partial",
          root_limit < geometry.root_sectors ? "sector-limit" :
          (geometry.type == 32 ? "first-cluster-boundary" : "fixed-root-end"), entries, lfn);
    return 0;
}
