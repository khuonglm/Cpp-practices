// Test driver for the M1 engine.
//   reference build:  -Ireference -Iinclude  (uses the correct reference)
//   skeleton build:   -Iskeleton  -Iinclude  (uses YOUR code; fails until implemented)
#include "ob/matching_engine.hpp"
#include "engine_tests.hpp"

int main() {
    return ob::test::run_all<ob::MatchingEngine>("MatchingEngine  (M1)");
}
