"""Exercise the logging macros taken from the actual DEV9 and ATAD sources."""
import os
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
fixture = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static char captured[1024];
static int trace_calls, stdout_calls;
static void pccardTrace(const char *format, ...)
{
    va_list args;
    ++trace_calls;
    va_start(args, format);
    vsnprintf(captured, sizeof(captured), format, args);
    va_end(args);
}
static int unexpectedPrintf(const char *format, ...)
{
    (void)format;
    ++stdout_calls;
    return 0;
}
#define printf unexpectedPrintf
#include "production_macros.h"
int main(void)
{
    M_PRINTF("CIS MANFID actual=%08lx expected=%08lx\n", 0x12345678UL, 0xf15300UL);
    assert(stdout_calls == 0 && trace_calls == 1);
    assert(strcmp(captured, MODNAME ": CIS MANFID actual=12345678 expected=00f15300\n") == 0);
    M_PRINTF("No device.\n");
    assert(stdout_calls == 0 && trace_calls == 2);
    assert(strcmp(captured, MODNAME ": No device.\n") == 0);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="pcdiag-macro-") as directory:
    temporary = Path(directory)
    (temporary / "test.c").write_text(fixture)
    for relative in ("iop/pcdev9/ps2dev9.c", "iop/pcatad/ps2atad.c"):
        source = (root / relative).read_text()
        definitions = []
        for name in ("MODNAME", "M_PRINTF"):
            match = re.search(r"^#define " + name + r"\b[^\n]*(?:\n[^\n]*)?", source, re.M)
            if match is None:
                raise AssertionError(f"Missing production macro {name} in {relative}")
            lines = match.group().splitlines()
            definitions.append(lines[0] + ("\n" + lines[1] if lines[0].rstrip().endswith("\\") else ""))
        (temporary / "production_macros.h").write_text("\n".join(definitions) + "\n")
        executable = temporary / "test"
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-fsanitize=address,undefined",
                        "-fno-omit-frame-pointer", "-g", str(temporary / "test.c"),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        print(f"PASS: {relative} sends formatted and argument-free messages to pccardTrace", flush=True)
