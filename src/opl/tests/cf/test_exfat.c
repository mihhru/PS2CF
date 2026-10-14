#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "../../modules/pccf/pccf_ata.c"
static FILE *image;
static unsigned int pos, remaining, sector_lba, ext_commands, flushes;
static unsigned char sector_buf[512];
static const unsigned int offset=0x20000000, volume_sectors=400000000;
static pccf_ata_t a;
static void tr(const char *s,...) {(void)s;}
static unsigned char rd8(void *c,unsigned int r){(void)c;(void)r;return remaining ? 0x58 : 0x50;}
static unsigned short rd16(void *c,unsigned int r){
 unsigned int v; (void)c; assert(r==0 && remaining);
 if(pos==0) assert(pread(fileno(image),sector_buf,512,(off_t)sector_lba*512)==512);
 v=sector_buf[pos]|sector_buf[pos+1]<<8;pos+=2;
 if(pos==512){pos=0;remaining--;sector_lba++;} return v;
}
static void access_mode(void *c,unsigned int s,unsigned int w){(void)c;(void)s;(void)w;}
static void wait_ms(void *c,unsigned int ms){(void)c;(void)ms;}
static int cmd(void *c,unsigned int op,unsigned int lba,unsigned int n){
 (void)c;
 if(op==0xea){assert(fdatasync(fileno(image))==0);flushes++;return 0;}
 assert(op==0x24 || op==0x34);ext_commands++;
 sector_lba=lba;remaining=n;pos=0;return 0;
}
static void wr_sector(const unsigned char *p){
 assert(remaining);assert(pwrite(fileno(image),p,512,(off_t)sector_lba*512)==512);
 sector_lba++;remaining--;
}
static const pcdiag_cf_ops_t io={rd8,rd16,access_mode,wait_ms};
static const pcdiag_ata_ops_t ops={&io,0};
DSTATUS disk_initialize(BYTE p){(void)p;return 0;}
DSTATUS disk_status(BYTE p){(void)p;return 0;}
DRESULT disk_read(BYTE p,BYTE *b,LBA_t l,UINT n){
 (void)p;if(l>=volume_sectors || n>volume_sectors-l)return RES_PARERR;
 while(n){unsigned take=n>64 ? 64:n;if(pccf_ata_read(&a,(unsigned int)l+offset,take,b))return RES_ERROR;b+=take*512;l+=take;n-=take;}return RES_OK;
}
DRESULT disk_write(BYTE p,const BYTE *b,LBA_t l,UINT n){
 (void)p;if(l>=volume_sectors || n>volume_sectors-l)return RES_PARERR;
 while(n){unsigned take=n>64 ? 64:n;if(pccf_ata_write(&a,(unsigned int)l+offset,take,b))return RES_ERROR;b+=take*512;l+=take;n-=take;}return RES_OK;
}
DRESULT disk_ioctl(BYTE p,BYTE cmd,void *b){
 (void)p;switch(cmd){case CTRL_SYNC:return RES_OK;case GET_SECTOR_COUNT:*(LBA_t*)b=volume_sectors;return RES_OK;case GET_SECTOR_SIZE:*(WORD*)b=512;return RES_OK;case GET_BLOCK_SIZE:*(DWORD*)b=1;return RES_OK;default:return RES_PARERR;}
}
int main(void){
 FATFS fs; FIL f; FILINFO info;UINT n;unsigned i;BYTE work[32768],data[8192],got[8192];
 image=tmpfile();assert(image);assert(ftruncate(fileno(image),(off_t)1000000000*512)==0);
 memset(&a,0,sizeof(a));a.ready=1;a.sectors=1000000000;a.lba48=1;a.flush_command=0xea;a.write_cache=1;
 a.ops=&ops;a.trace=tr;a.issue_command=cmd;a.write_sector=wr_sector;
 MKFS_PARM opt={FM_EXFAT|FM_SFD,1,0,0,524288};
 assert(f_mkfs("0:",&opt,work,sizeof(work))==FR_OK);
 assert(f_mount(&fs,"0:",1)==FR_OK && fs.fs_type==FS_EXFAT);
 assert(f_mkdir("0:/DVD")==FR_OK);
 assert(f_open(&f,"0:/DVD/Large ISO test.iso",FA_CREATE_NEW|FA_WRITE)==FR_OK);
 for(i=0;i<sizeof(data);i++)data[i]=(i*17+i/256)&255;
 assert(f_write(&f,data,sizeof(data),&n)==FR_OK && n==sizeof(data));
 const FSIZE_t tail=(FSIZE_t)5*1024*1024*1024;
 assert(f_lseek(&f,tail)==FR_OK && f_tell(&f)==tail);
 assert(f_write(&f,data,sizeof(data),&n)==FR_OK && n==sizeof(data));
 assert(f_close(&f)==FR_OK);assert(f_mount(NULL,"0:",0)==FR_OK);
 assert(f_mount(&fs,"0:",1)==FR_OK);
 assert(f_stat("0:/DVD/Large ISO test.iso",&info)==FR_OK && info.fsize==tail+sizeof(data));
 assert(f_open(&f,"0:/DVD/Large ISO test.iso",FA_READ)==FR_OK);
 assert(f_read(&f,got,sizeof(got),&n)==FR_OK && n==sizeof(got) && !memcmp(got,data,n));
 assert(f_lseek(&f,tail)==FR_OK);
 assert(f_read(&f,got,sizeof(got),&n)==FR_OK && n==sizeof(got) && !memcmp(got,data,n));
 assert(f_close(&f)==FR_OK);
 assert(f_rename("0:/DVD/Large ISO test.iso","0:/DVD/Renamed.iso")==FR_OK);
 assert(f_unlink("0:/DVD/Renamed.iso")==FR_OK);
 assert(f_unlink("0:/DVD")==FR_OK);
 assert(a.ready && ext_commands && flushes);
 printf("PASS FatFs exFAT through actual ATA R/W: volume beyond 128GiB, 5GiB+ file size/seek, create/read/flush/remount/rename/delete; ext=%u flush=%u\n",ext_commands,flushes);
 fclose(image);return 0;
}
