#include "Chaos.h"
#include "Check.h"

using namespace ca;

namespace {

void valueBlendsRanges() {
    const ChaosRange r{0, 0.2, 0, 1};
    for (int i = 0; i < 100; ++i) {
        const double roll = i / 100.0;
        const double at0 = chaosValue(r, 0, roll);
        const double at1 = chaosValue(r, 1, roll);
        CHECK(at0 >= 0 && at0 <= 0.2);
        CHECK(at1 >= 0 && at1 <= 1);
    }
    CHECK(chaosValue(r, 0.5, 0.999) > 0.55);
}

void stepCoversEveryStep() {
    const ChaosRange r{-2, 3, -4, 0};   // exponents
    bool seen[6] = {};
    for (int i = 0; i < 600; ++i) {
        const int s = chaosStep(r, 0, i / 600.0);
        CHECK(s >= -2 && s <= 3);
        if (s >= -2 && s <= 3) seen[s + 2] = true;
    }
    for (bool b : seen) CHECK(b);
    for (int i = 0; i < 100; ++i) {
        const int s = chaosStep(r, 1, i / 100.0);
        CHECK(s >= -4 && s <= 0);
    }
}

void clockCountsUnits() {
    ChaosClock c;
    c.restart(0);
    CHECK(!c.tick(0, 1));      // the origin is not a roll
    CHECK(!c.tick(0.5, 1));
    CHECK(c.tick(1, 1));
    CHECK(!c.tick(1.5, 1));
    CHECK(c.tick(2, 1));
    CHECK(!c.tick(2.5, 0.5));  // new unit size: count again, no roll
    CHECK(c.tick(3, 0.5));
    CHECK(!c.tick(3.2, 0));    // off
    CHECK(!c.tick(3.5, 0.5));  // back on: fresh count
    CHECK(c.tick(4, 0.5));

    c.restart(10.25);
    CHECK(!c.tick(10.25, 2));
    CHECK(!c.tick(12, 2));
    CHECK(c.tick(12.25, 2));
}

} // namespace

void chaosTests() {
    valueBlendsRanges();
    stepCoversEveryStep();
    clockCountsUnits();
}
