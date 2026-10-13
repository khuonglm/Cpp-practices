// M2 uses the SAME test-suite as M1 - proof that the optimized internals kept
// the public behaviour identical.
#include "ob/matching_engine_fast.hpp"
#include "engine_tests.hpp"

int main() {
    return ob::test::run_all<ob::MatchingEngineFast>("MatchingEngineFast  (M2)");
}
