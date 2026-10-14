#!/usr/bin/env bash
# Override these three paths to use SDKs installed elsewhere.
PS2CF_MODERN="${PS2CF_MODERN:-$HOME/ps2dev-v2.0.0}"
PS2CF_LEGACY_SDK="${PS2CF_LEGACY_SDK:-$HOME/ps2dev-v1.0}"
PS2CF_LEGACY_TOOLS="${PS2CF_LEGACY_TOOLS:-$HOME/ps2dev-v1.1}"
PS2CF_BASE_PATH="$PATH"
modern_env() {
    export PS2DEV="$PS2CF_MODERN"
    export PS2SDK="$PS2DEV/ps2sdk"
    export GSKIT="$PS2DEV/gsKit"
    export PATH="$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2DEV/bin:$PS2SDK/bin:$PS2CF_BASE_PATH"
    test -f "$PS2SDK/Defs.make" || { echo "Missing modern PS2SDK: $PS2SDK" >&2; return 1; }
}
legacy_env() {
    export PS2DEV="$PS2CF_LEGACY_SDK"
    export PS2SDK="$PS2DEV/ps2sdk"
    export GSKIT="$PS2DEV/gsKit"
    export PATH="$PS2CF_LEGACY_TOOLS/ee/bin:$PS2CF_LEGACY_TOOLS/iop/bin:$PS2CF_LEGACY_TOOLS/dvp/bin:$PS2CF_LEGACY_TOOLS/bin:$PS2CF_LEGACY_TOOLS/ps2sdk/bin:$PS2SDK/bin:$PS2CF_BASE_PATH"
    test -f "$PS2SDK/Defs.make" || { echo "Missing legacy PS2SDK: $PS2SDK" >&2; return 1; }
}
