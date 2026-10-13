#!/usr/bin/env bash
# Build & run every target. Tests use ASan/UBSan; concurrency also uses TSan; bench uses -O2.
# Skeleton targets are EXPECTED to fail until you implement them.
set -uo pipefail
cd "$(dirname "$0")"

CXX=${CXX:-clang++}
STD="-std=c++20"
WARN="-Wall -Wextra -Wpedantic"
SAN="-fsanitize=address,undefined -g"
TSAN="-fsanitize=thread -g"
INC="-Iinclude -Itests"
mkdir -p build

red(){ printf "\033[31m%s\033[0m\n" "$*"; }
grn(){ printf "\033[32m%s\033[0m\n" "$*"; }
hdr(){ printf "\n\033[1m======== %s ========\033[0m\n" "$*"; }

# hdr "M1 - matching engine"
# echo "[reference] (should PASS)"
# $CXX $STD $WARN $SAN $INC -Ireference tests/test_engine.cpp -o build/test_engine_ref && ./build/test_engine_ref
# echo "[skeleton]  (your code)"
# $CXX $STD $WARN $SAN $INC -Iskeleton  tests/test_engine.cpp -o build/test_engine_skel \
#     && ./build/test_engine_skel || red "  M1 skeleton not passing yet (expected)."

# hdr "M2 - optimized engine (intrusive list + pool)"
# echo "[reference] (should PASS)"
# $CXX $STD $WARN $SAN $INC -Ireference tests/test_engine_m2.cpp -o build/test_engine_m2 && ./build/test_engine_m2
# echo "[skeleton]  (your code)"
# $CXX $STD $WARN $SAN $INC -Iskeleton  tests/test_engine_m2.cpp -o build/test_engine_m2_skel \
#     && ./build/test_engine_m2_skel || red "  M2 skeleton not passing yet (expected)."

# hdr "M3 - SPSC lock-free queue"
# echo "[reference + ASan]"
# $CXX $STD $WARN $SAN  $INC -Ireference -pthread tests/test_spsc.cpp -o build/test_spsc_ref  && ./build/test_spsc_ref
# echo "[reference + ThreadSanitizer]"
# $CXX $STD $WARN $TSAN $INC -Ireference -pthread tests/test_spsc.cpp -o build/test_spsc_tsan && ./build/test_spsc_tsan
# echo "[skeleton + ASan]  (your code)"
# $CXX $STD $WARN $SAN  $INC -Iskeleton  -pthread tests/test_spsc.cpp -o build/test_spsc_skel \
#     && ./build/test_spsc_skel || red "  M3 skeleton not passing yet (expected)."
# echo "[skeleton + ThreadSanitizer]  (the memory_order net for YOUR queue - M3 done-criterion)"
# $CXX $STD $WARN $TSAN $INC -Iskeleton  -pthread tests/test_spsc.cpp -o build/test_spsc_skel_tsan \
#     && ./build/test_spsc_skel_tsan || red "  M3 skeleton TSan not passing yet (expected)."

hdr "M4 - latency benchmark"
$CXX $STD $WARN -O2 -DNDEBUG $INC -Ireference bench/benchmark.cpp -o build/benchmark && ./build/benchmark

# hdr "CLI"
# $CXX $STD $WARN -O2 $INC -Ireference tools/cli.cpp -o build/cli && grn "built ./build/cli  (run it and type commands)"

echo ""
grn "Done. Binaries in ./build/"
