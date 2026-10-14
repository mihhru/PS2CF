/* cf: IOMANX filesystem. All FatFs calls and the single PC CARD window are
   serialized by one semaphore. Fixed handle pool; no heap or card writes. */
#include <irx.h>
#include <loadcore.h>
#include <iomanX.h>
#include <thsemap.h>
#include <sysclib.h>
#include <errno.h>
#include "pcclog.h"
#include "pccf_ata.h"
#include "../pcdev9/pcdiag_fat.h"
#include "pccf_protocol.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"

IRX_ID("pc_card_cf", 1, 0);
#define CF_HANDLES 16
#define CF_PATH_BYTES 1025
/* Bound disk work per VFS call when a damaged FAT chain circles. FileXio
   reads in 128 KiB requests; healthy sequential files are not size-limited
   by this per-call counter. A pathological large seek may return EIO. */
#define CF_IO_SECTOR_BUDGET 8192U
typedef struct {
    int type;
    unsigned int bytes, entries, read_us;
    union { FIL file; DIR directory; } object;
} cf_handle_t;
static cf_handle_t handles[CF_HANDLES];
static FATFS filesystem;
static pccf_ata_t ata;
static FILINFO info; /* Shared only under lock: keep IOP stacks small. */
static pcdiag_fat_geometry_t geometry;
static unsigned char sector0[512], boot_sector[512];
static int mutex = -1, mounted;
static unsigned int budget, entry_logs;
PARTITION VolToPart[FF_VOLUMES] = {{0, 0}};

static int available(void)
{
    if (ata.ready && !pccf_hw_present()) {
        ata.ready = 0;
        pccardTrace("pccf: CARD REMOVED/offline; restart with the card inserted");
    }
    return ata.ready;
}
DSTATUS disk_status(BYTE drive)
{
    return drive == 0 && available() ? STA_PROTECT : (STA_NOINIT | STA_NODISK | STA_PROTECT);
}
DSTATUS disk_initialize(BYTE drive) { return disk_status(drive); }
DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t lba, UINT count)
{
    unsigned int take;
    if (drive || !buffer || !count || lba >= ata.sectors || count > ata.sectors - lba) return RES_PARERR;
    if (!available()) return RES_NOTRDY;
    if (count > budget) {
        pccardTrace("pccf: IO STOP per-call sector budget exhausted; possible damaged FAT chain");
        return RES_ERROR;
    }
    /* After validation, FatFs can read only MBR or the chosen volume. */
    if (geometry.total_sectors && !((lba == 0 && count == 1) ||
        (lba >= geometry.volume_base && lba - geometry.volume_base < geometry.total_sectors &&
         count <= geometry.total_sectors - (lba - geometry.volume_base)))) return RES_PARERR;
    budget -= count;
    while (count) {
        take = count > 64 ? 64 : count;
        if (pccf_ata_read(&ata, lba, take, buffer)) return RES_ERROR;
        lba += take; buffer += take * 512; count -= take;
    }
    return RES_OK;
}
DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t lba, UINT count)
{
    (void)drive; (void)buffer; (void)lba; (void)count; return RES_WRPRT;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
    if (drive) return RES_PARERR;
    if (!available()) return RES_NOTRDY;
    if (command == CTRL_SYNC) return RES_OK;
    if (!buffer) return RES_PARERR;
    switch (command) {
        case GET_SECTOR_COUNT: *(LBA_t *)buffer = ata.sectors; return RES_OK;
        case GET_SECTOR_SIZE: *(WORD *)buffer = 512; return RES_OK;
        case GET_BLOCK_SIZE: *(DWORD *)buffer = 1; return RES_OK;
        default: return RES_PARERR;
    }
}
static unsigned int le32(const unsigned char *b)
{
    return (unsigned int)b[0] | ((unsigned int)b[1] << 8) |
           ((unsigned int)b[2] << 16) | ((unsigned int)b[3] << 24);
}
static int choose_volume(void)
{
    unsigned int i, base, extent, type;
    const unsigned char *p;
    if (pccf_ata_read(&ata, 0, 1, sector0)) return -1;
    VolToPart[0].pt = 0;
    if (!pcdiag_fat_geometry(sector0, 0, ata.sectors, &geometry)) return 0;
    if (sector0[510] != 0x55 || sector0[511] != 0xaa) return -1;
    for (i = 0; i < 4; ++i) {
        p = sector0 + 446 + i * 16; type = p[4]; base = le32(p + 8); extent = le32(p + 12);
        if ((p[0] != 0 && p[0] != 0x80) || !(type == 1 || type == 4 || type == 6 || type == 0x0e || type == 0x0b || type == 0x0c) ||
            !base || base >= ata.sectors || !extent || extent > ata.sectors - base) continue;
        if (pccf_ata_read(&ata, base, 1, boot_sector)) return -1;
        if (!pcdiag_fat_geometry(boot_sector, base, extent, &geometry)) {
            VolToPart[0].pt = i + 1; return 0;
        }
    }
    return -1;
}
static int result_errno(FRESULT result)
{
    switch (result) {
        case FR_OK: return 0;
        case FR_NO_FILE: case FR_NO_PATH: return -ENOENT;
        case FR_INVALID_NAME: case FR_INVALID_PARAMETER: return -EINVAL;
        case FR_INVALID_OBJECT: return -EBADF;
        case FR_DENIED: return -EACCES;
        case FR_EXIST: return -EEXIST;
        case FR_WRITE_PROTECTED: return -EROFS;
        case FR_NOT_READY: return -ENODEV;
        case FR_TOO_MANY_OPEN_FILES: return -EMFILE;
        default: return -EIO;
    }
}
static int lock(void)
{
    if (mutex < 0 || WaitSema(mutex) < 0) return -EIO;
    budget = CF_IO_SECTOR_BUDGET; return 0;
}
static int unlock(int result) { SignalSema(mutex); return result; }
static cf_handle_t *free_handle(void)
{
    unsigned int i;
    for (i = 0; i < CF_HANDLES; ++i) if (!handles[i].type) return handles + i;
    return NULL;
}
static cf_handle_t *get_handle(iop_file_t *file, int type)
{
    unsigned int i;
    for (i = 0; i < CF_HANDLES; ++i)
        if (file->privdata == handles + i && handles[i].type == type) return handles + i;
    return NULL;
}
static int valid_path(iop_file_t *file, const char *path)
{
    unsigned int i;
    if (file->unit != 0) return -ENODEV;
    if (!path) return -EINVAL;
    for (i = 0; i < CF_PATH_BYTES; ++i) {
        if (!path[i]) return 0;
        if (path[i] == ':') return -EINVAL; /* No alternate FatFs drive prefix. */
    }
    return -ENAMETOOLONG;
}
static int root_path(const char *path)
{
    while (*path == '/' || *path == '\\') ++path;
    return !*path;
}
static void fat_time(unsigned char *out, unsigned int date, unsigned int time)
{
    unsigned int year;
    memset(out, 0, 8);
    if (!date) return;
    year = 1980 + (date >> 9);
    out[1] = (time & 31) * 2; out[2] = (time >> 5) & 63; out[3] = time >> 11;
    out[4] = date & 31; out[5] = (date >> 5) & 15; out[6] = year & 255; out[7] = year >> 8;
}
static void fill_stat(iox_stat_t *stat, const FILINFO *item)
{
    memset(stat, 0, sizeof(*stat));
    stat->mode = FIO_S_IRUSR | FIO_S_IRGRP | FIO_S_IROTH;
    if (item->fattrib & AM_DIR) stat->mode |= FIO_S_IFDIR | FIO_S_IXUSR | FIO_S_IXGRP | FIO_S_IXOTH;
    else { stat->mode |= FIO_S_IFREG; stat->size = item->fsize; }
    stat->attr = item->fattrib | AM_RDO;
    fat_time(stat->mtime, item->fdate, item->ftime);
    fat_time(stat->ctime, item->crdate ? item->crdate : item->fdate, item->crdate ? item->crtime : item->ftime);
    fat_time(stat->atime, item->fdate, 0);
}
static int cf_init(iop_device_t *device) { (void)device; return 0; }
static int cf_deinit(iop_device_t *device)
{
    unsigned int i;
    (void)device;
    if (lock()) return -EIO;
    for (i = 0; i < CF_HANDLES; ++i) if (handles[i].type) return unlock(-EBUSY);
    f_mount(NULL, "0:", 0); mounted = 0; ata.ready = 0; pccf_hw_stop();
    unlock(0); DeleteSema(mutex); mutex = -1; return 0;
}
static int cf_open(iop_file_t *file, const char *path, int flags, int mode)
{
    cf_handle_t *handle;
    int result;
    (void)mode;
    if ((flags & 3) != O_RDONLY || (flags & (O_APPEND | O_CREAT | O_TRUNC | O_EXCL))) return -EROFS;
    if ((result = valid_path(file, path))) return result;
    if (root_path(path)) return -EISDIR;
    if (lock()) return -EIO;
    if (!mounted || !available()) return unlock(-ENODEV);
    handle = free_handle(); if (!handle) return unlock(-EMFILE);
    result = result_errno(f_open(&handle->object.file, path, FA_READ));
    if (!result) {
        handle->type = 1; handle->bytes = 0; file->privdata = handle;
        pccardTrace("pccf: OPEN path=%s bytes=%lu", path, (unsigned long)f_size(&handle->object.file));
    } else pccardTrace("pccf: OPEN FAIL path=%s rc=%d", path, result);
    return unlock(result);
}
static int cf_close(iop_file_t *file)
{
    cf_handle_t *handle;
    if (lock()) return -EIO;
    handle = get_handle(file, 1); if (!handle) return unlock(-EBADF);
    if (available()) f_close(&handle->object.file);
    pccardTrace("pccf: CLOSE bytes_read=%lu", (unsigned long)handle->bytes);
    pccardTrace("pccf: READ timing bytes=%lu read_us=%lu (excludes USB writes)",
                (unsigned long)handle->bytes, (unsigned long)handle->read_us);
    pccardTrace("pccf: IO totals sectors=%lu commands=%lu multi_commands=%lu",
                (unsigned long)ata.sector_reads, (unsigned long)ata.commands, (unsigned long)ata.multi_commands);
    memset(handle, 0, sizeof(*handle)); file->privdata = NULL; return unlock(0);
}
static int cf_read(iop_file_t *file, void *buffer, int size)
{
    cf_handle_t *handle;
    UINT got = 0;
    unsigned int start_us = 0;
    int result;
    if (size < 0 || (size && !buffer)) return -EINVAL;
    if (lock()) return -EIO;
    handle = get_handle(file, 1); if (!handle) return unlock(-EBADF);
    if (!available()) return unlock(-ENODEV);
    if (ata.time_us) start_us = ata.time_us();
    result = result_errno(f_read(&handle->object.file, buffer, size, &got));
    if (ata.time_us) handle->read_us += ata.time_us() - start_us;
    handle->bytes += got;
    if (result) pccardTrace("pccf: READ FAIL rc=%d partial_bytes=%u", result, got);
    return unlock(got ? (int)got : result);
}
static s64 cf_seek(iop_file_t *file, s64 offset, int whence, int narrow)
{
    cf_handle_t *handle;
    s64 origin, target;
    int result;
    if (offset < -4294967295LL || offset > 4294967295LL) return -EINVAL;
    if (lock()) return -EIO;
    handle = get_handle(file, 1); if (!handle) return unlock(-EBADF);
    if (!available()) return unlock(-ENODEV);
    if (whence == SEEK_SET) origin = 0;
    else if (whence == SEEK_CUR) origin = f_tell(&handle->object.file);
    else if (whence == SEEK_END) origin = f_size(&handle->object.file);
    else return unlock(-EINVAL);
    target = origin + offset;
    if (target < 0 || target > 4294967295LL) return unlock(-EINVAL);
    if (target > (s64)f_size(&handle->object.file)) target = f_size(&handle->object.file);
    if (narrow && target > 2147483647LL) return unlock(-EOVERFLOW);
    result = result_errno(f_lseek(&handle->object.file, (FSIZE_t)target));
    if (result) return unlock(result);
    target = f_tell(&handle->object.file); unlock(0); return target;
}
static int cf_lseek(iop_file_t *file, int offset, int whence) { return (int)cf_seek(file, offset, whence, 1); }
static s64 cf_lseek64(iop_file_t *file, s64 offset, int whence) { return cf_seek(file, offset, whence, 0); }
static int cf_dopen(iop_file_t *file, const char *path)
{
    cf_handle_t *handle;
    int result;
    if ((result = valid_path(file, path))) return result;
    if (lock()) return -EIO;
    if (!mounted || !available()) return unlock(-ENODEV);
    handle = free_handle(); if (!handle) return unlock(-EMFILE);
    result = result_errno(f_opendir(&handle->object.directory, root_path(path) ? "/" : path));
    if (!result) {
        handle->type = 2; handle->entries = 0; file->privdata = handle;
        pccardTrace("pccf: DOPEN path=%s", path);
    } else pccardTrace("pccf: DOPEN FAIL path=%s rc=%d", path, result);
    return unlock(result);
}
static int cf_dclose(iop_file_t *file)
{
    cf_handle_t *handle;
    if (lock()) return -EIO;
    handle = get_handle(file, 2); if (!handle) return unlock(-EBADF);
    if (available()) f_closedir(&handle->object.directory);
    pccardTrace("pccf: DCLOSE entries=%u", handle->entries);
    memset(handle, 0, sizeof(*handle)); file->privdata = NULL; return unlock(0);
}
static int cf_dread(iop_file_t *file, iox_dirent_t *entry)
{
    cf_handle_t *handle;
    int result;
    if (!entry) return -EINVAL;
    if (lock()) return -EIO;
    handle = get_handle(file, 2); if (!handle) return unlock(-EBADF);
    if (!available()) return unlock(-ENODEV);
    result = result_errno(f_readdir(&handle->object.directory, &info));
    if (result) return unlock(result);
    if (!info.fname[0]) return unlock(0);
    memset(entry, 0, sizeof(*entry)); fill_stat(&entry->stat, &info);
    strncpy(entry->name, info.fname, sizeof(entry->name) - 1);
    ++handle->entries;
    if (entry_logs++ < 128) pccardTrace("pccf: DREAD name=%s type=%s bytes=%lu", entry->name,
        (info.fattrib & AM_DIR) ? "directory" : "file", (unsigned long)info.fsize);
    return unlock(1);
}
static int cf_getstat(iop_file_t *file, const char *path, iox_stat_t *stat)
{
    int result;
    if (!stat) return -EINVAL;
    if ((result = valid_path(file, path))) return result;
    if (lock()) return -EIO;
    if (!mounted || !available()) return unlock(-ENODEV);
    if (root_path(path)) { memset(&info, 0, sizeof(info)); info.fattrib = AM_DIR; result = 0; }
    else result = result_errno(f_stat(path, &info));
    if (!result) fill_stat(stat, &info);
    return unlock(result);
}
static int cf_devctl(iop_file_t *file, const char *name, int command, void *args, unsigned int arglen, void *buffer, unsigned int buflen)
{
    pccf_info_t *out = (pccf_info_t *)buffer;
    (void)name; (void)args; (void)arglen;
    if (file->unit) return -ENODEV;
    if (lock()) return -EIO;
    if (command == PCCF_DEVCTL_OFF) { ata.ready = 0; pccf_hw_stop(); return unlock(0); }
    if (command == PCCF_DEVCTL_INFO && buffer && buflen >= sizeof(*out)) {
        out->online = available(); out->sectors = ata.sectors;
        out->sector_reads = ata.sector_reads; out->fat_type = geometry.type; return unlock(0);
    }
    return unlock(-EINVAL);
}
/* Exact IOMANX signatures: no dummy function-pointer casts or media writes. */
static int cf_format(iop_file_t *f,const char *a,const char *b,void *c,int d) { (void)f;(void)a;(void)b;(void)c;(void)d;return -EROFS; }
static int cf_write(iop_file_t *f,void *a,int b) { (void)f;(void)a;(void)b;return -EROFS; }
static int cf_ioctl(iop_file_t *f,int a,void *b) { (void)f;(void)a;(void)b;return -EINVAL; }
static int cf_remove(iop_file_t *f,const char *a) { (void)f;(void)a;return -EROFS; }
static int cf_mkdir(iop_file_t *f,const char *a,int b) { (void)f;(void)a;(void)b;return -EROFS; }
static int cf_chstat(iop_file_t *f,const char *a,iox_stat_t *b,unsigned int c) { (void)f;(void)a;(void)b;(void)c;return -EROFS; }
static int cf_rename(iop_file_t *f,const char *a,const char *b) { (void)f;(void)a;(void)b;return -EROFS; }
static int cf_chdir(iop_file_t *f,const char *a) { (void)f;(void)a;return -EINVAL; }
static int cf_sync(iop_file_t *f,const char *a,int b) { (void)f;(void)a;(void)b;return 0; }
static int cf_mount(iop_file_t *f,const char *a,const char *b,int c,void *d,int e) { (void)f;(void)a;(void)b;(void)c;(void)d;(void)e;return -EINVAL; }
static int cf_umount(iop_file_t *f,const char *a) { (void)f;(void)a;return -EINVAL; }
static int cf_readlink(iop_file_t *f,const char *a,char *b,unsigned int c) { (void)f;(void)a;(void)b;(void)c;return -EINVAL; }
static int cf_ioctl2(iop_file_t *f,int a,void *b,unsigned int c,void *d,unsigned int e) { (void)f;(void)a;(void)b;(void)c;(void)d;(void)e;return -EINVAL; }
static iop_device_ops_t operations = {
    cf_init, cf_deinit, cf_format, cf_open, cf_close, cf_read, cf_write,
    cf_lseek, cf_ioctl, cf_remove, cf_mkdir, cf_remove, cf_dopen, cf_dclose,
    cf_dread, cf_getstat, cf_chstat, cf_rename, cf_chdir, cf_sync, cf_mount,
    cf_umount, cf_lseek64, cf_devctl, cf_rename, cf_readlink, cf_ioctl2
};
static iop_device_t device = {"cf", IOP_DT_FS | IOP_DT_FSEXT, 1, "PC CARD CompactFlash (read-only)", &operations};
int _start(int argc, char **argv)
{
    iop_sema_t sema;
    FRESULT result;
    (void)argc; (void)argv;
    memset(handles, 0, sizeof(handles)); memset(&geometry, 0, sizeof(geometry));
    mounted = 0; entry_logs = 0;
    if (pccf_hw_start(&ata)) return MODULE_NO_RESIDENT_END;
    if (choose_volume()) { pccardTrace("pccf: MOUNT FAIL: no bounded supported FAT12/16/32 volume"); goto fail; }
    budget = CF_IO_SECTOR_BUDGET;
    result = f_mount(&filesystem, "0:", 1);
    if (result || (filesystem.fs_type != FS_FAT12 && filesystem.fs_type != FS_FAT16 && filesystem.fs_type != FS_FAT32) ||
        filesystem.volbase != geometry.volume_base || filesystem.csize == 0) {
        pccardTrace("pccf: MOUNT FAIL FatFs rc=%d", (int)result); goto fail;
    }
    memset(&sema, 0, sizeof(sema)); sema.attr = 0; sema.initial = sema.max = 1;
    mutex = CreateSema(&sema); if (mutex < 0) goto fail;
    mounted = 1;
    if (AddDrv(&device) < 0) { mounted = 0; DeleteSema(mutex); mutex = -1; goto fail; }
    pccardTrace("pccf: READY device=cf: FAT%u base=%08lx sectors=%08lx handles=%u; read-only, LFN=UTF-8",
                geometry.type, (unsigned long)geometry.volume_base, (unsigned long)geometry.total_sectors, CF_HANDLES);
    return MODULE_RESIDENT_END;
fail:
    f_mount(NULL, "0:", 0); ata.ready = 0; pccf_hw_stop(); return MODULE_NO_RESIDENT_END;
}
