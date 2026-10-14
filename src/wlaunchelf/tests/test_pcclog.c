#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#define _start pcclog_module_start
#include "../iop/pcclog/pcclog.c"
#undef _start
struct irx_export_table _exp_pcclog;
static void check(const char *expected,const char *fmt,...)
{
    char out[384]; va_list ap;
    va_start(ap,fmt); format_line(out,sizeof(out),fmt,ap); va_end(ap);
    assert(strcmp(out,expected)==0);
}
static void bounded(char *out,unsigned int n,const char *fmt,...)
{
    va_list ap; va_start(ap,fmt); format_line(out,n,fmt,ap); va_end(ap);
}
int main(void)
{
    char guard[32], huge[2048];
    unsigned int offset;
    pcdiag_packet_t *p;
    check("MANFID=00f15300 rc=-19 byte=FF", "MANFID=%08lx rc=%d byte=%02X", 0xf15300UL,-19,255U);
    check("0 4294967295 -2147483648", "%u %u %d",0U,UINT_MAX,INT_MIN);
    check("00500  text % (null)", "%05u %5s %% %s",500U," text",(char *)0);
    memset(huge,'A',sizeof(huge));huge[sizeof(huge)-1]=0;
    memset(guard,'Z',sizeof(guard));
    bounded(guard+8,8,"%s",huge);
    assert(memcmp(guard,"ZZZZZZZZ",8)==0);
    assert(memcmp(guard+16,"ZZZZZZZZZZZZZZZZ",16)==0);
    assert(guard[15]==0);
    used=dropped=0;
    pccardTrace("actual=%08lx expected=%08lx rc=%d",0x12345678UL,0xf15300UL,-19);
    assert(used<sizeof(trace));trace[used]=0;
    assert(strstr(trace,"[1.023 IOP] actual=12345678 expected=00f15300 rc=-19\n"));
    offset=0;p=snapshot(1,&offset,4);
    assert(p->magic==PCDIAG_MAGIC && p->count==used && p->offset==0);
    assert(memcmp(p->data,trace,p->count)==0);
    offset=used+100;p=snapshot(1,&offset,4);
    assert(p->offset==used && p->count==0);
    for (offset=0;offset<1000;++offset) pccardTrace("%s",huge);
    assert(used<=sizeof(trace) && dropped>0);
    offset=0;p=snapshot(1,&offset,4);
    assert(p->count==sizeof(p->data) && p->dropped==dropped);
    puts("PASS: formatting, string bounds, time prefix, RPC pages, full-buffer accounting");
    return 0;
}
