from pathlib import Path
import re, subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'modules/pccf/pccf_hw.c').read_text()
funcs='\n'.join(re.search(r'static (?:int|void) '+name+r'\(.*?\n}',s,re.S).group() for name in ['issue_rw','pio_write_sector'])
pre=r'''
#include <assert.h>
#include <stdio.h>
#define PCDIAG_ATA_LBA28_SECTORS 0x10000000U
#define REG(n) (n)
#define CARD(n) (0x10000+(n))
static unsigned int regs[32], vals[32], n, stores, timing, pio_settle_us, selector;
static int present=1;
static unsigned char bytes[513];
static unsigned int rd16(unsigned int a) {(void)a;return 0;}
static unsigned int rd32(unsigned int a) {(void)a;return 0;}
static unsigned int rd8(unsigned int a) {(void)a;return 0x50;}
static void wr8(unsigned int a,unsigned int b) {assert(n<32);regs[n]=a-0x10000;vals[n++]=b;}
static void wr32(unsigned int a,unsigned int b) {assert(a==0x1418);timing=b;}
static void wr16(unsigned int a,unsigned int b) {
 if(a==0x1460){assert(b==0);return;}
 assert(a==CARD(0) && stores<256 && timing==0x001a10bb);
 assert(b==(unsigned int)(bytes[stores*2+1] | bytes[stores*2+2]<<8));stores++;
}
#define HW_R8 rd8
#define HW_R16 rd16
#define HW_R32 rd32
#define HW_W8 wr8
#define HW_W16 wr16
#define HW_W32 wr32
static int pccf_hw_present(void){return present;}
static void DelayThread(unsigned int x){(void)x;}
static int issue_read_count(void *c,unsigned int cmd,unsigned int l,unsigned int count){(void)c;(void)l;(void)count;assert(cmd==0x20);return 77;}
'''
post=r'''
int main(void){
 unsigned int i;
 assert(issue_rw(0,0x34,0x30012345,2)==0 && n==10);
 unsigned int wantreg[]={6,2,3,4,5,2,3,4,5,7};
 unsigned int wantval[]={0xe0,0,0x30,0,0,2,0x45,0x23,1,0x34};
 for(i=0;i<n;i++) assert(regs[i]==wantreg[i] && vals[i]==wantval[i]);
 n=0; assert(issue_rw(0,0x24,0xfffffffe,1)==0 && vals[2]==255 && vals[6]==254 && vals[9]==0x24);
 n=0; assert(issue_rw(0,0x30,0x01234567,1)==0 && n==6 && vals[0]==0xe1 && vals[2]==0x67 && vals[5]==0x30);
 n=0; assert(issue_rw(0,0x30,0x10000000,1)<0 && !n);
 assert(issue_rw(0,0x34,0,0)<0 && !n); assert(issue_rw(0,0x34,0,65)<0 && !n);
 assert(issue_rw(0,0xff,0,1)<0 && !n);
 assert(issue_rw(0,0xea,0,0)==0 && n==1 && vals[0]==0xea);
 n=0;present=0;assert(issue_rw(0,0x34,0,1)<0 && !n);present=1;
 assert(issue_rw(0,0x20,0,1)==77);
 for(i=0;i<sizeof(bytes);i++)bytes[i]=i*31;
 pio_write_sector(bytes+1);assert(stores==256);
 puts("PASS actual MMIO routines: LBA48 high-first order, full 32-bit LBA, legacy head bits, guards, 256 conservative stores, unaligned input");
}
'''
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'hw.c';exe=Path(d)/'hw';src.write_text(pre+funcs+post)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
