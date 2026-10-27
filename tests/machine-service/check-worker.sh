#!/usr/bin/env bash
# Real Lua worker and C-ABI fixture; native settlement remains a transport fixture.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${SAIVAGE_CARD_WORK_ROOT:?set a task-owned work root}"
area="$SAIVAGE_CARD_WORK_ROOT/lua-worker-check"
mkdir -p "$area"
# Do not write fixture scripts into the source checkout.
cd "$area"
ulimit -c 0
lua_package=${LUA_PACKAGE:-lua5.4}
# pkg-config output intentionally supplies compiler/linker argument lists.
read -r -a cflags <<< "$(pkg-config --cflags "$lua_package")"
read -r -a libs <<< "$(pkg-config --libs "$lua_package")"
"${CXX:-c++}" -std=c++17 -pthread -I"$root/include" \
    "${cflags[@]}" "$root/tests/machine-service/worker.cpp" \
    "$root/src/debug/lua_driver_worker.cpp" "${libs[@]}" -o worker
"${CXX:-c++}" -std=c++17 -shared -fPIC "${cflags[@]}" \
    "$root/tests/machine-service/worker-native.cpp" "${libs[@]}" -o worker_native.so
WORKER_CPATH="$area/?.so" ./worker
printf '%s\n' 'Lua worker/C-module transport fixture PASS (not production native integration)'
