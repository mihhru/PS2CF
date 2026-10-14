/* PC CARD DIAG 5. PIO data-in only, based on Transcend CF133 manual.
   Actual DIAG4 hardware log confirms SPACE=0 and correct byte/word lanes.
   A conservative reset signature is rechecked before the first command.
   Single initialization caller; static buffers avoid large IOP stack frames. */
#include "pcdiag_ata.h"
#include "pcdiag_fat.h"

typedef struct {
    const pcdiag_ata_ops_t *ops;
    void *hardware;
    pcdiag_cis_trace_t trace;
    unsigned int card_sectors, reads;
} ata_session_t;
static unsigned char identify[512], sector0[512];

static unsigned int word(const unsigned char *buffer, unsigned int index)
{
    return (unsigned int)buffer[index * 2] | ((unsigned int)buffer[index * 2 + 1] << 8);
}
static void ascii_field(const unsigned char *buffer, unsigned int first,
                         unsigned int words, char *output)
{
    unsigned int i, count = words * 2;
    for (i = 0; i < count; ++i) {
        unsigned int c = buffer[first * 2 + (i ^ 1U)];
        output[i] = c >= 32 && c <= 126 ? (char)c : '?';
    }
    while (count && output[count - 1] == ' ') --count;
    output[count] = 0;
}
static void dump_bytes(const char *tag, const unsigned char *buffer,
                       pcdiag_cis_trace_t trace)
{
    static const char hex[] = "0123456789abcdef";
    char line[48];
    unsigned int at, i;
    for (at = 0; at < 512; at += 16) {
        for (i = 0; i < 16; ++i) {
            line[i * 3] = hex[buffer[at + i] >> 4];
            line[i * 3 + 1] = hex[buffer[at + i] & 15];
            line[i * 3 + 2] = ' ';
        }
        line[47] = 0;
        trace("dev9: CF %s DATA offset=%03x bytes=%s", tag, at, line);
    }
}
static int wait_status(ata_session_t *session, int data_ready, const char *phase)
{
    const pcdiag_cf_ops_t *memory = session->ops->memory;
    unsigned int elapsed, status = 0;
    for (elapsed = 0; elapsed < PCDIAG_ATA_POLL_LIMIT_MS; ++elapsed) {
        status = memory->read8(session->hardware, 0x0e);
        if (status == 0 || status == 0xff) {
            session->trace("dev9: CF ATA FAIL phase=%s floating/absent status=%02x waited_ms=%u", phase, status, elapsed);
            return -1;
        }
        if (!(status & 0x80)) {
            if (status & 0x21) {
                session->trace("dev9: CF ATA FAIL phase=%s status=%02x error=%02x waited_ms=%u",
                               phase, status, (unsigned int)memory->read8(session->hardware, 1), elapsed);
                return -2;
            }
            if (data_ready ? ((status & 8) != 0) : (!(status & 8) && (status & 0x40))) {
                session->trace("dev9: CF ATA WAIT phase=%s status=%02x waited_ms=%u", phase, status, elapsed);
                return 0;
            }
        }
        memory->wait_ms(session->hardware, 1);
    }
    session->trace("dev9: CF ATA TIMEOUT phase=%s status=%02x limit_ms=%u", phase, status, PCDIAG_ATA_POLL_LIMIT_MS);
    return -3;
}
static int transfer(ata_session_t *session, unsigned int command,
                     unsigned int lba, unsigned char *buffer)
{
    const pcdiag_cf_ops_t *memory = session->ops->memory;
    unsigned int i, value;
    int result;
    memory->set_access(session->hardware, 0, 8);
    result = wait_status(session, 0, "before-command");
    if (result) return result;
    session->trace("dev9: CF ATA COMMAND cmd=%02x lba=%08lx sector_count=1; PIO data-in", command, (unsigned long)lba);
    if (session->ops->issue_read_command(session->hardware, command, lba)) {
        session->trace("dev9: CF ATA FAIL: host rejected command=%02x lba=%08lx", command, (unsigned long)lba);
        return -4;
    }
    /* Four slow Alternate Status reads cover ATA command settling time. */
    for (i = 0; i < 4; ++i) memory->read8(session->hardware, 0x0e);
    result = wait_status(session, 1, "data-request");
    if (result) return result;
    memory->set_access(session->hardware, 0, 16);
    for (i = 0; i < 256; ++i) {
        value = memory->read16(session->hardware, 0);
        buffer[i * 2] = value & 0xff;
        buffer[i * 2 + 1] = value >> 8;
    }
    memory->set_access(session->hardware, 0, 8);
    for (i = 0; i < 4; ++i) memory->read8(session->hardware, 0x0e);
    result = wait_status(session, 0, "transfer-complete");
    if (result) return result;
    value = memory->read8(session->hardware, 7); /* Acknowledge command IRQ. */
    if ((value & 0xa9) || !(value & 0x40)) {
        session->trace("dev9: CF ATA FAIL: final status=%02x", value);
        return -5;
    }
    session->trace("dev9: CF ATA TRANSFER OK cmd=%02x lba=%08lx words=256 final_status=%02x", command, (unsigned long)lba, value);
    return 0;
}
static int read_sector(void *context, unsigned int lba, unsigned char *buffer)
{
    ata_session_t *session = (ata_session_t *)context;
    if (lba >= session->card_sectors || lba >= PCDIAG_ATA_LBA28_SECTORS ||
        session->reads >= PCDIAG_ATA_READ_LIMIT) {
        session->trace("dev9: CF ATA READ STOP lba=%08lx capacity=%08lx reads=%u limit=%u",
                       (unsigned long)lba, (unsigned long)session->card_sectors, session->reads, PCDIAG_ATA_READ_LIMIT);
        return -1;
    }
    ++session->reads;
    return transfer(session, 0x20, lba, buffer);
}
int pcdiag_ata_observe(const pcdiag_ata_ops_t *ops, void *context,
                        pcdiag_cis_trace_t trace)
{
    const pcdiag_cf_ops_t *memory = ops->memory;
    ata_session_t session;
    unsigned int status, alt, sc, sn, cl, ch, dh, pair, i, sum = 0;
    unsigned int configuration, capabilities, sectors, valid106;
    char model[41], serial[21], firmware[9];
    int result;
    session.ops = ops; session.hardware = context; session.trace = trace;
    session.card_sectors = session.reads = 0;
    memory->set_access(context, 0, 8);
    status = memory->read8(context, 7); alt = memory->read8(context, 0x0e);
    sc = memory->read8(context, 2); sn = memory->read8(context, 3);
    cl = memory->read8(context, 4); ch = memory->read8(context, 5); dh = memory->read8(context, 6);
    memory->set_access(context, 0, 16);
    pair = memory->read16(context, 6);
    memory->set_access(context, 0, 8);
    trace("dev9: CF ATA PRECHECK SPACE=0 ST=%02x ALT=%02x SC=%02x SN=%02x CL=%02x CH=%02x DH=%02x PAIR06=%04x",
          status, alt, sc, sn, cl, ch, dh, pair);
    if (status != alt || status != (pair >> 8) || dh != (pair & 0xff) ||
        status == 0xff || !(status & 0x40) || (status & 0xa9) ||
        sc != 1 || sn != 1 || cl != 0 || ch != 0 || dh != 0xa0) {
        trace("dev9: CF ATA SKIP: reset signature or byte/word mapping differs; no command sent");
        return 1;
    }
    trace("dev9: CF IDENTIFY BEGIN cmd=ec SPACE=0; only read commands EC/20 are available");
    result = transfer(&session, 0xec, 0, identify);
    if (result) return result;
    dump_bytes("IDENTIFY", identify, trace);
    configuration = word(identify, 0); capabilities = word(identify, 49);
    sectors = word(identify, 60) | (word(identify, 61) << 16);
    ascii_field(identify, 27, 20, model);
    ascii_field(identify, 10, 10, serial);
    ascii_field(identify, 23, 4, firmware);
    for (i = 0; i < 512; ++i) sum += identify[i];
    trace("dev9: CF IDENTIFY model=%s serial=%s firmware=%s", model, serial, firmware);
    trace("dev9: CF IDENTIFY word0=%04x word49=%04x LBA_sectors=%08lx MiB=%lu integrity_signature=%02x checksum_mod256=%02x",
          configuration, capabilities, (unsigned long)sectors, (unsigned long)(sectors / 2048), (unsigned int)identify[510], sum & 0xff);
    if (configuration == 0 || configuration == 0xffff ||
        ((configuration & 0x8000) && configuration != 0x848a) ||
        !(capabilities & 0x0200) || sectors == 0 || sectors == 0xffffffffU ||
        (identify[510] == 0xa5 && (sum & 0xff))) {
        trace("dev9: CF IDENTIFY REJECT: configuration/LBA/capacity/integrity invalid; no sector read");
        return -6;
    }
    valid106 = word(identify, 106);
    if ((valid106 & 0xc000) == 0x4000 && (valid106 & 0x1000) &&
        (word(identify, 117) != 256 || word(identify, 118) != 0)) {
        trace("dev9: CF IDENTIFY REJECT: logical sector size differs from 512 bytes");
        return -7;
    }
    session.card_sectors = sectors > PCDIAG_ATA_LBA28_SECTORS ? PCDIAG_ATA_LBA28_SECTORS : sectors;
    trace("dev9: CF IDENTIFY ACCEPTED; accessible_LBA28_sectors=%08lx", (unsigned long)session.card_sectors);
    result = read_sector(&session, 0, sector0);
    if (result) return result;
    trace("dev9: CF SECTOR0 READ OK bytes=512 signature=%02x%02x", (unsigned int)sector0[510], (unsigned int)sector0[511]);
    dump_bytes("SECTOR0", sector0, trace);
    result = pcdiag_fat_preview(sector0, session.card_sectors, read_sector, &session, trace);
    trace("dev9: CF ATA OBSERVE END sector_reads=%u FAT_preview_rc=%d; hdd0 driver was not installed", session.reads, result);
    return result < 0 ? result : 0;
}
