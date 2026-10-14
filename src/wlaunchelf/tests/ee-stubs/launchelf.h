#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "pcdiag.h"
typedef uint8_t u8;
typedef uint64_t u64;
typedef struct { void *server; } SifRpcClientData_t;
typedef struct { int unused; } iox_stat_t;
#define ULE_VERSION "host-test"
u64 Timer(void);
int SifExecModuleBuffer(void *, int, int, const char *, int *);
int SifBindRpc(SifRpcClientData_t *,int,int);
int SifCallRpc(SifRpcClientData_t *,int,int,void *,int,void *,int,void *,void *);
int fileXioGetStat(const char *,iox_stat_t *);
int fileXioOpen(const char *,int,int);
int fileXioClose(int);
int fileXioWrite(int,const void *,int);
int fileXioDevctl(const char *,int,void *,unsigned int,void *,unsigned int);
int fileXioDopen(const char *);
