#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/test_driver_logging.py
task_test_dir=$(mktemp -d)
trap 'rm -rf "$task_test_dir"' EXIT HUP INT TERM
cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    tests/test_cis.c iop/pcdev9/pcdiag_cis.c -o "$task_test_dir/test_cis"
"$task_test_dir/test_cis"
cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    tests/test_cf.c iop/pcdev9/pcdiag_cf.c iop/pcdev9/pcdiag_cis.c -o "$task_test_dir/test_cf"
"$task_test_dir/test_cf"
python3 - "$task_test_dir/actual_cf_host.h" <<'PY'
import pathlib, sys
source = pathlib.Path('iop/pcdev9/ps2dev9.c').read_text()
start = source.index('typedef struct {\n\tu16 space;\n\tu32 timing;\n} pcdiag_cf_host_t;')
end = source.index('static int card_find_manfid(u32 manfid)', start)
pathlib.Path(sys.argv[1]).write_text(source[start:end])
PY
cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I"$task_test_dir" tests/test_cf_host.c iop/pcdev9/pcdiag_cf.c iop/pcdev9/pcdiag_cis.c \
    -o "$task_test_dir/test_cf_host"
"$task_test_dir/test_cf_host"
for test in ata fat; do
    cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
        "tests/test_$test.c" iop/pcdev9/pcdiag_ata.c iop/pcdev9/pcdiag_fat.c \
        -o "$task_test_dir/test_$test"
    "$task_test_dir/test_$test"
done
for name in irx loadcore intrman sifrpc sysclib thbase; do
    printf '#include "mock.h"\n' > "$task_test_dir/$name.h"
done
cc -std=gnu99 -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I"$task_test_dir" -Itests/iop-stubs -Iinclude tests/test_pcclog.c \
    -o "$task_test_dir/test_pcclog"
"$task_test_dir/test_pcclog"
cc -std=gnu99 -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -Itests/ee-stubs -Iinclude tests/test_pcdiag_ee.c \
    -o "$task_test_dir/test_pcdiag_ee"
"$task_test_dir/test_pcdiag_ee"
python3 tests/test_pccf_ee.py
cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    tests/test_pccf_ata.c iop/pccf/pccf_ata.c -o "$task_test_dir/test_pccf_ata"
"$task_test_dir/test_pccf_ata"
cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -Itests/pccf-stubs -Iiop/pcclog tests/test_pccf_hw.c \
    iop/pcdev9/pcdiag_cf.c iop/pcdev9/pcdiag_cis.c -o "$task_test_dir/test_pccf_hw"
"$task_test_dir/test_pccf_hw"
cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -DPCCF_SETTLE_US=100 -Itests/pccf-stubs -Iiop/pcclog tests/test_pccf_hw.c \
    iop/pcdev9/pcdiag_cf.c iop/pcdev9/pcdiag_cis.c -o "$task_test_dir/test_pccf_hw_fast"
"$task_test_dir/test_pccf_hw_fast"
cc -std=gnu99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -Itests/pccf-stubs -Iiop/pcclog -Iinclude tests/test_pccf_fs.c \
    iop/pccf/fatfs/ff.c iop/pccf/fatfs/ffunicode.c iop/pcdev9/pcdiag_fat.c \
    -o "$task_test_dir/test_pccf_fs"
"$task_test_dir/test_pccf_fs"
