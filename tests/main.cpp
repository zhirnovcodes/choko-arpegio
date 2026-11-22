#include "Check.h"

int gFailures = 0;

int main() {
    sequencesTests();
    timeTableTests();
    chaosTests();
    engineTests();
    algorithmsTests();
    if (gFailures == 0) {
        std::printf("ca_tests: all passed\n");
        return 0;
    }
    std::printf("ca_tests: %d failed\n", gFailures);
    return 1;
}
