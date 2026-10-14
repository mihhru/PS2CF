#ifndef PCDIAG_FAT_H
#define PCDIAG_FAT_H
#include "pcdiag_cis.h"

#define PCDIAG_FAT_ROOT_SECTOR_LIMIT 4U
typedef int (*pcdiag_read_sector_t)(void *context, unsigned int lba,
                                    unsigned char *sector512);
typedef struct {
    unsigned int type, volume_base, total_sectors, clusters;
    unsigned int root_lba, root_sectors;
} pcdiag_fat_geometry_t;

/* Metadata controls only bounded reads; no FAT chain or directory recursion.
   Returns 0 for a supported and internally consistent BPB, else 1. */
int pcdiag_fat_geometry(const unsigned char *boot, unsigned int base,
                         unsigned int extent, pcdiag_fat_geometry_t *geometry);
/* FAT12/16 fixed root or first FAT32 root cluster; first primary FAT partition
   or unpartitioned FAT volume. 8.3 aliases, no LFN reconstruction. */
int pcdiag_fat_preview(const unsigned char *sector0, unsigned int card_sectors,
                        pcdiag_read_sector_t read_sector, void *context,
                        pcdiag_cis_trace_t trace);
#endif
