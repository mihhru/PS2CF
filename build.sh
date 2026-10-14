#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
root="$PWD"
mkdir -p output
# ZIP extracted on Windows does not preserve executable bits for helper scripts.
find src -type f -name '*.sh' -exec chmod +x {} +
source tools/toolchains.sh
modern_env
cd src/opl
make -j2 languages DEBUG=0 EESIO_DEBUG=0 INGAME_DEBUG=0 PADEMU=0 REVISION=0 GIT_HASH=ps2cf DIRTY= OPL_VERSION=OPL-CF-v1
make -j2 opl_stripped.elf DEBUG=0 EESIO_DEBUG=0 INGAME_DEBUG=0 PADEMU=0 REVISION=0 GIT_HASH=ps2cf DIRTY= OPL_VERSION=OPL-CF-v1
opl_dir="$PWD"
(cd "$PS2DEV/share/ps2-packer/module" && ps2-packer -s ../stub/lzma-1d00-stub "$opl_dir/opl_stripped.elf" "$root/output/OPL_CF_v1.ELF")
cd "$root/src/wlaunchelf"
export CF_SDK="$PS2SDK"
make -C iop/cf_bdm all
make -C iop/pccf all
make -C iop/cf_bdmfs all
legacy_env
# libcdvd's EE archive is needed even when its legacy IRX was included upstream.
make -C iop/oldlibs/libcdvd all
make -j2 PCCARD_CF=1 EXFAT=1 ETH=0 UNC-BOOT-CF_EXFAT_RW-EXFAT-NO_NETWORK.ELF
wle_dir="$PWD"
modern_env
(cd "$PS2DEV/share/ps2-packer/module" && ps2-packer -s ../stub/lzma-1d00-stub "$wle_dir/UNC-BOOT-CF_EXFAT_RW-EXFAT-NO_NETWORK.ELF" "$root/output/wLaunchELF_CF_v1.ELF")
cd "$root"
python3 tools/verify.py
(cd output && sha256sum OPL_CF_v1.ELF wLaunchELF_CF_v1.ELF > SHA256SUMS.ELF)
echo 'PS2CF 1.0 ELF build and verification completed.'
