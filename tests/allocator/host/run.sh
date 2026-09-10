#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
mkdir -p "$here/build"
"${CC:-cc}" -std=c11 -g -O1 -Wall -Wextra -Werror -pthread \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$here" -I"$root/include" "$root/source/nxvm.c" "$here/failure.c" \
 -o "$here/build/failure-test"
"$here/build/failure-test"
