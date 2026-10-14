#ifndef PCDIAG_TEST_MOCK_H
#define PCDIAG_TEST_MOCK_H
#include <stdint.h>
#include <string.h>
typedef uint32_t u32;
typedef struct { u32 hi, lo; } iop_sys_clock_t;
typedef struct { int unused; } SifRpcDataQueue_t;
typedef struct { int unused; } SifRpcServerData_t;
typedef struct {
    int attr, priority, stacksize, option;
    void (*thread)(void *);
} iop_thread_t;
struct irx_export_table { int unused; };
#define IRX_ID(a,b,c)
#define TH_C 1
#define MODULE_NO_RESIDENT_END 1
#define MODULE_RESIDENT_END 0
static int GetSystemTime(iop_sys_clock_t *c) { c->hi=1; c->lo=23000; return 0; }
static void SysClock2USec(iop_sys_clock_t *c,u32 *s,u32 *u) { *s=c->hi; *u=c->lo; }
static void CpuSuspendIntr(int *s) { *s=1; }
static void CpuResumeIntr(int s) { (void)s; }
static int RegisterLibraryEntries(void *p) { (void)p; return 0; }
static void ReleaseLibraryEntries(void *p) { (void)p; }
static int CreateThread(void *p) { (void)p; return 1; }
static int StartThread(int id,void *p) { (void)id;(void)p; return 0; }
static void DeleteThread(int id) { (void)id; }
static int GetThreadId(void) { return 1; }
static void SifInitRpc(int v) { (void)v; }
static void SifSetRpcQueue(void *q,int v) { (void)q;(void)v; }
static void SifRegisterRpc(void *s,int id,void *f,void *b,void *c,void *d,void *q)
{ (void)s;(void)id;(void)f;(void)b;(void)c;(void)d;(void)q; }
static void SifRpcLoop(void *q) { (void)q; }
#endif
