#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${PS2DEV:?Set PS2DEV to the ps2dev toolchain directory}"
: "${PS2SDK:?Set PS2SDK to the PS2SDK directory}"
make clean PCCARD_DIAG=1 PCCARD_CF=1 ETH=0 EXFAT=0
for module in pcclog pcdev9 pcatad pccf; do make -C "iop/$module" clean; done
cat > githash.h <<'EOF'
#define ULE_VERDATE "PC CARD CF READONLY 2026-10-12"
#define GIT_HASH "pccf-12"
EOF
make -j2 PCCARD_DIAG=1 PCCARD_CF=1 ETH=0 EXFAT=0 UNC-BOOT-PCCARD_DIAG-CF_READONLY-NO_NETWORK.ELF
cp UNC-BOOT-PCCARD_DIAG-CF_READONLY-NO_NETWORK.ELF WLE_PCCARD_CF.ELF
