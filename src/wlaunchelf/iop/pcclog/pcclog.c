/* Diagnostic transport only: this module does not touch PC CARD registers. */
#include <irx.h>
#include <loadcore.h>
#include <intrman.h>
#include <sifrpc.h>
#include <sysclib.h>
#include <thbase.h>
#include <stdarg.h>
#include "pcclog.h"
#include "pcdiag_protocol.h"

IRX_ID("pc_card_log", 1, 1);
extern struct irx_export_table _exp_pcclog;
static char trace[PCDIAG_IOP_CAP];
static unsigned int used, dropped;
static SifRpcDataQueue_t queue;
static SifRpcServerData_t server;
static pcdiag_packet_t packet __attribute__((aligned(64)));

/* Bounded formatter for the integer/string formats used by DEV9 and ATAD.
   No ROM vsprintf: a corrupt/long string cannot overrun the line buffer. */
static void putc_b(char **p, char *end, char c)
{
    if (*p < end) *(*p)++ = c;
}
static void number(char **p, char *end, unsigned long n, unsigned int base,
                   unsigned int width, int zero, int negative, int upper)
{
    char digits[32];
    const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    unsigned int len = 0, total;
    do { digits[len++] = hex[n % base]; n /= base; } while (n && len < 32);
    total = len + (negative ? 1 : 0);
    if (negative && zero) putc_b(p, end, '-');
    while (width > total) { putc_b(p, end, zero ? '0' : ' '); --width; }
    if (negative && !zero) putc_b(p, end, '-');
    while (len) putc_b(p, end, digits[--len]);
}
static unsigned int format_line(char *out, unsigned int size,
                                const char *fmt, va_list ap)
{
    char *p = out, *end = out + size - 1;
    while (*fmt && p < end) {
        unsigned int width = 0;
        int zero = 0, islong = 0;
        char kind;
        if (*fmt != '%') { putc_b(&p, end, *fmt++); continue; }
        ++fmt;
        if (*fmt == '%') { putc_b(&p, end, *fmt++); continue; }
        if (*fmt == '0') { zero = 1; ++fmt; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt++ - '0');
            if (width > 32) width = 32;
        }
        if (*fmt == 'l') { islong = 1; ++fmt; }
        kind = *fmt;
        if (!kind) break;
        ++fmt;
        if (kind == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && p < end) putc_b(&p, end, *s++);
        } else if (kind == 'c') {
            putc_b(&p, end, (char)va_arg(ap, int));
        } else if (kind == 'd' || kind == 'i') {
            long n = islong ? va_arg(ap, long) : va_arg(ap, int);
            unsigned long mag = n < 0 ? 0UL - (unsigned long)n : (unsigned long)n;
            number(&p, end, mag, 10, width, zero, n < 0, 0);
        } else if (kind == 'u' || kind == 'x' || kind == 'X') {
            unsigned long n = islong ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            number(&p, end, n, kind == 'u' ? 10 : 16, width, zero, 0, kind == 'X');
        } else {
            putc_b(&p, end, '%'); putc_b(&p, end, kind);
        }
    }
    *p = 0;
    return (unsigned int)(p - out);
}
void pccardTrace(const char *fmt, ...)
{
    char line[384], prefix[48], *p = prefix;
    unsigned int len, plen;
    u32 sec = 0, usec = 0;
    iop_sys_clock_t now;
    int state;
    va_list ap;
    GetSystemTime(&now);
    SysClock2USec(&now, &sec, &usec);
    putc_b(&p, prefix + 47, '[');
    number(&p, prefix + 47, sec, 10, 0, 0, 0, 0);
    putc_b(&p, prefix + 47, '.');
    number(&p, prefix + 47, usec / 1000, 10, 3, 1, 0, 0);
    putc_b(&p, prefix + 47, ' ');
    putc_b(&p, prefix + 47, 'I'); putc_b(&p, prefix + 47, 'O');
    putc_b(&p, prefix + 47, 'P'); putc_b(&p, prefix + 47, ']');
    putc_b(&p, prefix + 47, ' ');
    plen = (unsigned int)(p - prefix);
    va_start(ap, fmt);
    len = format_line(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (!len || line[len - 1] != '\n') line[len++] = '\n';
    CpuSuspendIntr(&state);
    if (plen + len <= sizeof(trace) - used) {
        memcpy(trace + used, prefix, plen); used += plen;
        memcpy(trace + used, line, len); used += len;
    } else { ++dropped; }
    CpuResumeIntr(state);
}
static void *snapshot(int command, void *data, int size)
{
    unsigned int offset = size >= 4 ? *(unsigned int *)data : 0;
    int state;
    memset(&packet, 0, sizeof(packet));
    packet.magic = PCDIAG_MAGIC;
    packet.flags = 1;
    CpuSuspendIntr(&state);
    packet.used = used; packet.dropped = dropped;
    if (offset > used) offset = used;
    packet.offset = offset;
    packet.count = used - offset;
    if (packet.count > sizeof(packet.data)) packet.count = sizeof(packet.data);
    memcpy(packet.data, trace + offset, packet.count);
    CpuResumeIntr(state);
    return &packet;
}
static void rpc_thread(void *arg)
{
    SifInitRpc(0);
    SifSetRpcQueue(&queue, GetThreadId());
    SifRegisterRpc(&server, PCDIAG_RPC_ID, snapshot, &packet, 0, 0, &queue);
    SifRpcLoop(&queue);
}
int _start(int argc, char **argv)
{
    iop_thread_t t;
    int id;
    if (RegisterLibraryEntries(&_exp_pcclog) != 0) return MODULE_NO_RESIDENT_END;
    memset(&t, 0, sizeof(t));
    t.attr = TH_C; t.thread = rpc_thread; t.priority = 40; t.stacksize = 0x1000;
    id = CreateThread(&t);
    if (id < 0 || StartThread(id, 0) < 0) {
        if (id >= 0) DeleteThread(id);
        ReleaseLibraryEntries(&_exp_pcclog);
        return MODULE_NO_RESIDENT_END;
    }
    pccardTrace("PC CARD CF 12: transport ready; IOP time is seconds since reset");
    return MODULE_RESIDENT_END;
}
