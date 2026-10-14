#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/toolchains.sh
cache="${PS2CF_DOWNLOAD_CACHE:-$PWD/toolchains/downloads}"
mkdir -p "$cache"
fetch() {
    local url="$1" dest="$2" expected="$3"
    if [ ! -f "$dest" ]; then curl --fail --location --retry 3 "$url" -o "$dest"; fi
    printf '%s  %s\n' "$expected" "$dest" | sha256sum -c -
}
if [ ! -d "$PS2CF_MODERN" ]; then
    fetch https://github.com/ps2dev/ps2dev/releases/download/v2.0.0/ps2dev-ubuntu-latest.tar.gz "$cache/modern.tar.gz" c8e5dedccf62084d476894e88cd0451b57f1ed288e741eb3c1263010aaffc028
    mkdir -p "$PS2CF_MODERN"
    tar -xzf "$cache/modern.tar.gz" -C "$PS2CF_MODERN" --strip-components=1
fi
if [ ! -d "$PS2CF_LEGACY_TOOLS" ]; then
    fetch https://github.com/ps2dev/ps2dev/releases/download/v1.1/ps2dev-ubuntu-latest.tar.gz "$cache/legacy-tools.tar.gz" 6bd7352ff526239e928f5200b43afa96bdef04ecf48d15386bac954938b514a1
    mkdir -p "$PS2CF_LEGACY_TOOLS"
    tar -xzf "$cache/legacy-tools.tar.gz" -C "$PS2CF_LEGACY_TOOLS" --strip-components=1
fi
if [ ! -d "$PS2CF_LEGACY_SDK" ]; then
    skopeo copy docker://docker.io/ps2dev/ps2dev@sha256:da02eb0870afcfdb7198195de40d04cd121bd893668c7342fb84b3c4d273e9d1 "dir:$cache/image-v1.0"
    python3 tools/extract-legacy-sdk.py "$cache/image-v1.0" "$PS2CF_LEGACY_SDK"
    # Legacy utility executables in the 1.0 image use musl; use the glibc 1.1
    # copies via PATH. The SDK itself supplies headers, libraries and Makefiles.
fi
python3 - "$PS2CF_LEGACY_SDK" "$PS2CF_LEGACY_TOOLS" <<'PY'
from pathlib import Path
import sys
sdk, tools = map(Path, sys.argv[1:])
relative = Path('ee/lib/gcc-lib/ee/3.2.3/specs')
source, dest = sdk / relative, tools / relative
assert source.is_file() and dest.is_file(), 'Expected GCC 3.2.3 specs'
backup = dest.with_suffix('.ps2cf-original')
if not backup.exists():
    backup.write_bytes(dest.read_bytes())
dest.write_text(source.read_text().replace('-lps2sdkc', ''))
PY
echo 'PS2CF toolchains are ready. Run: bash build.sh'
