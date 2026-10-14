from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'modules/pccf/main.c').read_text()
func=re.search(r'static int cf_write\(.*?\n}',s,re.S).group()
pre=r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
typedef unsigned long long u64;typedef unsigned short u16;
struct block_device {int unused;};
static struct {unsigned int sectors,ready;} ata;
static unsigned char data[130*512+1];
static int io_lock, locked, calls, failure, present=1;
static unsigned int sizes[3],lbas[3];
static void WaitSema(int s){(void)s;assert(!locked);locked=1;}
static void SignalSema(int s){(void)s;assert(locked);locked=0;}
static int pccf_hw_present(void){return present;}
static int pccf_ata_write(void *a,unsigned int lba,unsigned int count,const unsigned char *p){
 (void)a;assert(locked && count<=64 && calls<3 && p==data+1+calls*64*512);
 sizes[calls]=count;lbas[calls++]=lba;
 if(calls==failure){ata.ready=0;return -1;}return 0;
}
'''
post=r'''
int main(void){
 ata.sectors=1000000000;ata.ready=1;
 assert(cf_write(0,0x30000000,data+1,130)==130 && !locked && calls==3);
 assert(sizes[0]==64 && sizes[1]==64 && sizes[2]==2);
 assert(lbas[0]==0x30000000 && lbas[2]==0x30000080);
 calls=0;failure=2;
 assert(cf_write(0,0x30000000,data+1,130)==-EIO && calls==2 && !locked && !ata.ready);
 assert(cf_write(0,0,data+1,1)==-EIO && calls==2);
 ata.ready=1; calls=0;failure=0;
 assert(cf_write(0,((u64)1<<32)+16,data+1,1)==-EINVAL && !calls);
 assert(cf_write(0,ata.sectors-1,data+1,2)==-EINVAL && !calls);
 assert(cf_write(0,0,0,1)==-EINVAL && !calls);
 present=0;assert(cf_write(0,0,data+1,1)==-EIO && !calls && !ata.ready && !locked);
 puts("PASS GUI block writes: serialization, 64/64/2 splitting, high LBA preservation, no 64-bit truncation, removal/failure latch");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'bd.c';exe=Path(d)/'bd';p.write_text(pre+func+post)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
