#ifndef PCDIAG_H
#define PCDIAG_H
void pcdiag_init(void);
void pcdiag_log(const char *format, ...);
void pcdiag_poll(void);
void pcdiag_flush(void);
void pcdiag_usb_ready(void);
const char *pcdiag_status(void);
int pcdiag_exec_module(const char *name, void *buffer, int size,
                       int argsize, const char *args, int *result);
int pcdiag_devctl(const char *name, int command, void *args, unsigned int arglen,
                  void *buf, unsigned int buflen);
int pcdiag_dopen(const char *path);
#endif
