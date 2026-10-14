#include "launchelf.h"
#include "pcdiag_protocol.h"

extern u8 pcclog_irx[];
extern int size_pcclog_irx;
extern char ROMVER_data[16];
#define EE_TRACE_CAP 131072
static char trace[EE_TRACE_CAP];
static unsigned int used, written, lost;
static unsigned int iop_offset, iop_dropped;
static int transport_loaded, bound, usb_ready;
static char path[64], status[96];
static SifRpcClientData_t client __attribute__((aligned(64)));
static pcdiag_packet_t reply __attribute__((aligned(64)));
static unsigned int request[16] __attribute__((aligned(64)));

static void append(const char *data, unsigned int size)
{
    if (size > sizeof(trace) - used) { ++lost; return; }
    memcpy(trace + used, data, size); used += size;
}
void pcdiag_log(const char *fmt, ...)
{
    char line[512], prefix[48];
    int len, plen;
    va_list ap;
    plen = snprintf(prefix, sizeof(prefix), "[%lu EE] ", (unsigned long)Timer());
    va_start(ap, fmt);
    len = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (len < 0) return;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    if (!len || line[len - 1] != '\n') line[len++] = '\n';
    append(prefix, plen); append(line, len);
}
void pcdiag_init(void)
{
    int id, result = -999;
    memset(&client, 0, sizeof(client));
    bound = 0; iop_offset = 0; iop_dropped = 0;
    id = SifExecModuleBuffer(pcclog_irx, size_pcclog_irx, 0, NULL, &result);
    transport_loaded = id >= 0 && result == 0;
    pcdiag_log("PC CARD CF 12; %s; SDK driver source 4a2d422a1824cd2374a41726884d9b470015aa2c", ULE_VERSION);
    pcdiag_log("EE timestamps: milliseconds since EE startup; IOP timestamps: seconds since IOP reset");
    pcdiag_log("pcclog: module_id=%d start_return=%d", id, result);
    pcdiag_poll();
}
void pcdiag_poll(void)
{
    int i, rc;
    unsigned int pages = 0;
    if (!transport_loaded) return;
    if (!bound) {
        for (i = 0; i < 128; ++i) {
            volatile int delay;
            rc = SifBindRpc(&client, PCDIAG_RPC_ID, 0);
            if (rc < 0) break;
            if (client.server != NULL) { bound = 1; break; }
            for (delay = 0; delay < 10000; ++delay) asm volatile("nop");
        }
        if (!bound) { pcdiag_log("pcclog: RPC not ready; will retry at next checkpoint"); return; }
    }
    do {
        request[0] = iop_offset;
        rc = SifCallRpc(&client, 1, 0, request, 4, &reply, sizeof(reply), NULL, NULL);
        if (rc < 0 || reply.magic != PCDIAG_MAGIC || reply.count > sizeof(reply.data) ||
            reply.offset != iop_offset || reply.used > PCDIAG_IOP_CAP ||
            reply.offset > reply.used ||
            reply.count > reply.used - reply.offset) {
            pcdiag_log("pcclog: invalid snapshot or RPC failure rc=%d", rc); break;
        }
        append(reply.data, reply.count);
        iop_offset += reply.count;
        if (reply.dropped != iop_dropped) {
            iop_dropped = reply.dropped;
            pcdiag_log("pcclog: IOP buffer full, dropped_lines=%u", iop_dropped);
        }
    } while (iop_offset < reply.used && ++pages < 40);
}
void pcdiag_flush(void)
{
    int fd, rc, i;
    iox_stat_t st;
    if (!usb_ready) return;
    if (!path[0]) {
        for (i = 0; i < 100; ++i) {
            snprintf(path, sizeof(path), "mass:/PCCARD%02d.LOG", i);
            rc = fileXioGetStat(path, &st);
            if (rc == -ENOENT) {
                fd = fileXioOpen(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
                if (fd >= 0) { fileXioClose(fd); break; }
            }
        }
        if (i == 100) { path[0] = 0; return; }
    }
    fd = fileXioOpen(path, O_WRONLY | O_APPEND, 0);
    if (fd < 0) return;
    while (written < used) {
        rc = fileXioWrite(fd, trace + written, used - written);
        if (rc <= 0) break;
        written += rc;
    }
    fileXioClose(fd);
}
/* Called once the LaunchELF USB drivers have been loaded. */
void pcdiag_usb_ready(void)
{
    usb_ready = 1;
    pcdiag_log("USB log checkpoint; ROMVER=%.14s", ROMVER_data);
    pcdiag_poll(); pcdiag_flush();
}
const char *pcdiag_status(void)
{
    if (path[0] && written == used)
        snprintf(status, sizeof(status), "PC CARD log: %s", path);
    else
        snprintf(status, sizeof(status), "PC CARD log pending: check FAT32 USB (mass:)");
    return status;
}
int pcdiag_exec_module(const char *name, void *buffer, int size,
                       int argsize, const char *args, int *result)
{
    int id, ret = -999;
    u64 begin;
    pcdiag_log("BEGIN module %s bytes=%d", name, size);
    pcdiag_poll(); pcdiag_flush();
    begin = Timer();
    id = SifExecModuleBuffer(buffer, size, argsize, args, &ret);
    pcdiag_poll();
    pcdiag_log("END module %s: module_id=%d start_return=%d elapsed_ms=%lu", name, id, ret, (unsigned long)(Timer() - begin));
    if (result) *result = ret;
    pcdiag_flush();
    return id;
}
int pcdiag_devctl(const char *name, int command, void *args, unsigned int arglen,
                  void *buf, unsigned int buflen)
{
    int rc;
    u64 begin;
    pcdiag_log("BEGIN devctl %s cmd=0x%08x", name, command);
    pcdiag_flush();
    begin = Timer();
    rc = fileXioDevctl(name, command, args, arglen, buf, buflen);
    pcdiag_poll();
    pcdiag_log("END devctl %s cmd=0x%08x rc=%d elapsed_ms=%lu", name, command, rc, (unsigned long)(Timer() - begin));
    pcdiag_flush();
    return rc;
}
int pcdiag_dopen(const char *name)
{
    int rc;
    u64 begin;
    pcdiag_log("BEGIN directory %s", name); pcdiag_flush();
    begin = Timer();
    rc = fileXioDopen(name);
    pcdiag_poll();
    pcdiag_log("END directory %s fd_or_error=%d elapsed_ms=%lu", name, rc, (unsigned long)(Timer() - begin));
    pcdiag_flush();
    return rc;
}
