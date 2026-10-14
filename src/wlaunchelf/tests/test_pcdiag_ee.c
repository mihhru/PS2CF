#include <assert.h>
#include "../src/pcdiag.c"
u8 pcclog_irx[1];
int size_pcclog_irx = 1;
char ROMVER_data[16] = "0100JC20000117";
static char files[2][EE_TRACE_CAP];
static unsigned int lengths[2], cursor[2];
static int exists[2], fail_writes;
static const char iop_log[] = "[1.000 IOP] dev9: MANFID mismatch\n";
u64 Timer(void) { static u64 t; return ++t; }
int SifExecModuleBuffer(void *b,int size,int args,const char *p,int *ret)
{ (void)size;(void)args;(void)p; *ret = b==pcclog_irx ? 0 : 1; return 42; }
int SifBindRpc(SifRpcClientData_t *c,int id,int mode)
{ (void)id;(void)mode;c->server=(void *)1;return 0; }
int SifCallRpc(SifRpcClientData_t *c,int f,int mode,void *send,int slen,void *recv,int rlen,void *cb,void *p)
{
    pcdiag_packet_t *r=recv;
    unsigned int off=*(unsigned int *)send;
    (void)c;(void)f;(void)mode;(void)slen;(void)rlen;(void)cb;(void)p;
    memset(r,0,sizeof(*r));r->magic=PCDIAG_MAGIC;r->used=sizeof(iop_log)-1;
    r->offset=off;r->count=r->used-off;
    memcpy(r->data,iop_log+off,r->count);return 0;
}
static int index_of(const char *name)
{ int n=-1;assert(sscanf(name,"mass:/PCCARD%2d.LOG",&n)==1);assert(n>=0 && n<2);return n; }
int fileXioGetStat(const char *name,iox_stat_t *st)
{ (void)st;return exists[index_of(name)] ? 0 : -ENOENT; }
int fileXioOpen(const char *name,int flags,int mode)
{
    int n=index_of(name);(void)mode;
    if ((flags&O_CREAT) && (flags&O_EXCL) && exists[n]) return -EEXIST;
    if (!exists[n] && !(flags&O_CREAT)) return -ENOENT;
    exists[n]=1;cursor[n]=(flags&O_APPEND) ? lengths[n] : 0;return n+10;
}
int fileXioClose(int fd) { (void)fd;return 0; }
int fileXioWrite(int fd,const void *data,int count)
{
    int n=fd-10;
    if (fail_writes && lengths[n]>=51) return -EIO;
    if (count>17) count=17; /* Deliberate short write. */
    assert(cursor[n]+count<=sizeof(files[n]));
    memcpy(files[n]+cursor[n],data,count);cursor[n]+=count;
    if (cursor[n]>lengths[n]) lengths[n]=cursor[n];return count;
}
int fileXioDevctl(const char *n,int c,void *a,unsigned int al,void *b,unsigned int bl)
{ (void)n;(void)c;(void)a;(void)al;(void)b;(void)bl;return -19; }
int fileXioDopen(const char *name) { (void)name;return -19; }
int main(void)
{
    unsigned int before;
    int result;
    memcpy(files[0],"EXISTING",8);lengths[0]=8;exists[0]=1;
    pcdiag_init();assert(written==0);
    fail_writes=1;pcdiag_usb_ready();
    assert(strcmp(path,"mass:/PCCARD01.LOG")==0);
    assert(written>0 && written<used);
    fail_writes=0;pcdiag_flush();
    assert(written==used && lengths[1]==used);
    assert(memcmp(files[1],trace,used)==0);
    assert(lengths[0]==8 && memcmp(files[0],"EXISTING",8)==0);
    before=used;pcdiag_poll();assert(used==before); /* No duplicated IOP lines. */
    pcdiag_log("later line");pcdiag_flush();
    assert(lengths[1]==used && memcmp(files[1],trace,used)==0);
    assert(pcdiag_exec_module("DEV9",(void *)2,1,0,NULL,&result)==42 && result==1);
    assert(pcdiag_devctl("hdd0:",0x4801,NULL,0,NULL,0)==-19);
    assert(pcdiag_dopen("hdd0:")==-19);
    assert(strstr(files[1],"MANFID mismatch"));
    assert(strstr(files[1],"start_return=1"));
    assert(strstr(files[1],"rc=-19"));
    for (before=0;before<10000;++before) pcdiag_log("fill=%u",before);
    assert(used<=EE_TRACE_CAP && lost>0);
    puts("PASS: new log filename, partial writes/retry, append, IOP polling, module/error records, EE buffer bound");
    return 0;
}
