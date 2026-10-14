"""Exercise actual EE loading/poweroff functions: DEV9 must not disable CF."""
import os
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = (root / "src/main.c").read_text()


def function(name):
    source = (root / ("src/filer.c" if name in ("genClose", "genDclose") else "src/main.c")).read_text()
    match = re.search(r"^(?:static )?(?:void|int) " + re.escape(name) + r"\([^;\n]*\)\n\{", source, re.M)
    assert match, name
    begin = match.start()
    pos = source.index("{", begin) + 1
    depth = 1
    while depth:
        # These functions contain no braces inside strings/comments.
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[begin:pos] + "\n"


fixture = r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "pccf_protocol.h"
#define PC_CARD_CF 1
#define PC_CARD_DIAG 1
#define TRUE 1
#define DPRINTF(...) ((void)0)
#define LNG(text) #text
#define PDIOC_CLOSEALL 10
#define DDIOC_OFF 11
#define USBMASS_DEVCTL_STOP_ALL 12
static int cf_module_loaded, cf_module_attempted, ps2dev9_loaded, have_ps2dev9;
static int done_setupPowerOff, have_poweroff, have_HDD_modules, is_early_init;
static unsigned char pccf_irx[1], ps2dev9_irx[1], poweroff_irx[1];
static int size_pccf_irx=1, size_ps2dev9_irx=1, size_poweroff_irx=1;
static int cf_calls, dev9_calls, atad_calls, poweroff_calls, inits, callbacks;
static int logs, flushes, polls, messages, cf_return, cf_id, shut;
static char last_message[128], order[16];
static int order_size;
static int module(const char *name, void *data, int size, int argc, const char *args, int *ret)
{
    assert(size == 1 && argc == 0 && args == NULL);
    if (!strcmp(name,"PCCF")) { assert(data == pccf_irx); ++cf_calls; *ret=cf_return; return cf_id; }
    assert(!strcmp(name,"DEV9") && data == ps2dev9_irx);
    ++dev9_calls; *ret=0; return 4;
}
#define PCDIAG_MODULE module
static int SifExecModuleBuffer(void *data, int size, int argc, const char *args, int *ret)
{
    assert(data == poweroff_irx && size == 1 && argc == 0 && !args);
    ++poweroff_calls; *ret=0; return 5;
}
static void drawMsg(const char *message) { ++messages; strcpy(last_message,message); }
static void pcdiag_log(const char *format, ...) { (void)format; ++logs; }
static void pcdiag_flush(void) { ++flushes; }
static void pcdiag_poll(void) { ++polls; }
static int fileXioClose(int fd) { assert(fd==27); return -EIO; }
static int fileXioDclose(int fd) { assert(fd==29); return 0; }
static void poweroffInit(void) { ++inits; }
static void poweroffHandler(int i) { (void)i; }
static void poweroffSetCallback(void *fn, void *arg) { assert(fn==(void *)poweroffHandler && !arg); ++callbacks; }
static void poweroffShutdown(void) { ++shut; order[order_size++]='S'; }
static int fileXioDevctl(const char *name, int cmd, void *in, unsigned int inlen, void *out, unsigned int outlen)
{
    assert(!in && !inlen && !out && !outlen);
    if (!strcmp(name,"cf:")) { assert(cmd==PCCF_DEVCTL_OFF); order[order_size++]='C'; }
    else if (!strcmp(name,"mass:")) { assert(cmd==USBMASS_DEVCTL_STOP_ALL); order[order_size++]='U'; }
    else if (!strcmp(name,"pfs:")) { assert(cmd==PDIOC_CLOSEALL); order[order_size++]='P'; }
    else { assert(!strcmp(name,"dev9x:") && cmd==DDIOC_OFF); order[order_size++]='D'; }
    return 0;
}
static void load_ps2atad(void) { ++atad_calls; }
static void setupPowerOff(void);
#include "actual_pccf_ee.h"
static void reset(void)
{
    cf_module_loaded=cf_module_attempted=ps2dev9_loaded=have_ps2dev9=0;
    done_setupPowerOff=have_poweroff=have_HDD_modules=is_early_init=0;
    cf_calls=dev9_calls=atad_calls=poweroff_calls=inits=callbacks=0;
    logs=flushes=polls=messages=shut=order_size=0;
    cf_return=0; cf_id=3; last_message[0]=0;
}
int main(void)
{
    reset(); assert(loadCfModules()==0);
    assert(cf_module_loaded && cf_module_attempted && cf_calls==1);
    assert(poweroff_calls==1 && inits==1 && callbacks==1 && done_setupPowerOff);
    assert(dev9_calls==0 && !ps2dev9_loaded && !have_ps2dev9);
    assert(loadCfModules()==0 && cf_calls==1 && poweroff_calls==1);
    load_ps2dev9(); loadHddModules();
    assert(dev9_calls==0 && atad_calls==0 && !have_HDD_modules);
    closeAllAndPoweroff();
    assert(shut==1 && order_size==3 && !memcmp(order,"CUS",3));
    for (int mode=0; mode<2; ++mode) {
        reset(); if (mode) cf_id=-1; else cf_return=1;
        assert(loadCfModules()==-ENODEV && !cf_module_loaded && cf_module_attempted);
        assert(cf_calls==1 && dev9_calls==0 && poweroff_calls==0);
        assert(strstr(last_message,"CF unavailable"));
        assert(loadCfModules()==-ENODEV && cf_calls==1);
    }
    reset(); load_ps2dev9();
    assert(ps2dev9_loaded && have_ps2dev9 && dev9_calls==1);
    assert(loadCfModules()==-ENODEV && cf_calls==0 && !cf_module_attempted);
    loadHddModules(); assert(atad_calls==1 && have_HDD_modules);
    closeAllAndPoweroff(); assert(order_size==4 && !memcmp(order,"PDUS",4));
    reset(); is_early_init=1;
    assert(loadCfModules()==0 && messages==0 && logs==1 && flushes==1);
    assert(genClose(27)==-EIO && polls==1 && flushes==2);
    assert(genDclose(29)==0 && polls==2 && flushes==3);
    puts("PASS: actual EE CF cache, exclusive ownership, no DEV9/ATAD after CF, shutdown order, early-init behavior and HDD fallback");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="pccf-ee-") as directory:
    temporary = Path(directory)
    names = ("load_ps2dev9", "closeAllAndPoweroff", "setupPowerOff", "loadCfModules", "loadHddModules", "genClose", "genDclose")
    (temporary / "actual_pccf_ee.h").write_text("\n".join(map(function, names)))
    (temporary / "test.c").write_text(fixture)
    executable = temporary / "test"
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-variable", "-Wno-unused-but-set-variable", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", "-g", "-I"+str(temporary), "-I"+str(root / "include"),
                    str(temporary / "test.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
