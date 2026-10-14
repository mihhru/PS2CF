#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
for name in bd profile ata_rw; do
    cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/cf/stubs "tests/cf/test_$name.c" -o "$test_dir/$name"
    "$test_dir/$name"
done
