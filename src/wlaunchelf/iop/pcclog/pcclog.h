#ifndef PCCLOG_H
#define PCCLOG_H
#include <irx.h>
void pccardTrace(const char *format, ...);
#define pcclog_IMPORTS_start DECLARE_IMPORT_TABLE(pcclog, 1, 1)
#define pcclog_IMPORTS_end END_IMPORT_TABLE
#define I_pccardTrace DECLARE_IMPORT(4, pccardTrace)
#endif
