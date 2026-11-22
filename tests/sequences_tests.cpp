#include "Check.h"
#include "Sequences.h"

using namespace ca;

namespace {

bool isPow2(int v) { return v > 0 && (v & (v - 1)) == 0; }
// A power-of-two length of beats in quanta: 3 · 2^k.
bool isLength(int v) { return v % 3 == 0 && isPow2(v / 3); }

void timesFillTheLoop() {
    for (int loopExp = -2; loopExp <= 5; ++loopExp) {
        for (int minExp = -4; minExp <= 4; ++minExp) {
            for (int maxExp = -4; maxExp <= 4; ++maxExp) {
                for (std::uint64_t seed = 1; seed <= 5; ++seed) {
                    Rng rng(seed * 7919 + loopExp * 31 + minExp * 7 + maxExp);
                    Times t;
                    generateTimes(rng, loopExp, minExp, maxExp, t);
                    const int lo = minExp < maxExp ? minExp : maxExp;
                    int hi = minExp < maxExp ? maxExp : minExp;
                    if (hi > loopExp) hi = loopExp;
                    int sum = 0;
                    CHECK(t.count >= 1);
                    CHECK(t.count <= kMaxSlots);
                    for (int i = 0; i < t.count; ++i) {
                        CHECK(isLength(t.quanta[i]));
                        if (lo < loopExp) {
                            CHECK(t.quanta[i] >= quantaOfExp(lo));
                            CHECK(t.quanta[i] <= quantaOfExp(hi));
                        }
                        sum += t.quanta[i];
                    }
                    CHECK_EQ(sum, quantaOfExp(loopExp));
                    if (lo >= loopExp) CHECK_EQ(t.count, 1);
                }
            }
        }
    }
}

void timesUseEveryLength() {
    // Loop 4, lengths 1/4..1: over many seeds every allowed length shows up.
    bool seen[3] = {};
    for (std::uint64_t seed = 1; seed < 50; ++seed) {
        Rng rng(seed);
        Times t;
        generateTimes(rng, 2, -2, 0, t);
        for (int i = 0; i < t.count; ++i) {
            if (t.quanta[i] == quantaOfExp(-2)) seen[0] = true;
            if (t.quanta[i] == quantaOfExp(-1)) seen[1] = true;
            if (t.quanta[i] == quantaOfExp(0)) seen[2] = true;
        }
    }
    CHECK(seen[0] && seen[1] && seen[2]);
}

void maxSlots() {
    Rng rng(3);
    Times t;
    generateTimes(rng, 5, -4, -4, t);   // 32 beats of 1/16
    CHECK_EQ(t.count, kMaxSlots);
}

void swingHasOneValuePerPair() {
    Rng rng(9);
    Swing s;
    generateSwing(rng, 6, s);
    for (int i = 0; i < 5; ++i) {
        CHECK(s.value[i] >= -1 && s.value[i] < 1);
        CHECK(s.value[i] != 0);
    }
    for (int i = 5; i < kMaxSlots; ++i) CHECK_EQ(s.value[i], 0.0);
}

void sustainInRange() {
    Rng rng(4);
    Sustain s;
    generateSustain(rng, 100, -3, 1, s);   // swapped order is fine too
    for (int i = 0; i < 100; ++i) {
        CHECK(isLength(s.quanta[i]));
        CHECK(s.quanta[i] >= quantaOfExp(-3));
        CHECK(s.quanta[i] <= quantaOfExp(1));
    }
    Rng rng2(4);
    Sustain swapped;
    generateSustain(rng2, 100, 1, -3, swapped);
    for (int i = 0; i < 100; ++i) CHECK_EQ(swapped.quanta[i], s.quanta[i]);
    CHECK_EQ(s.quanta[100], 0);
}

void notesInUnitRange() {
    Rng rng(5);
    Notes n;
    generateNotes(rng, 64, n);
    for (int i = 0; i < 64; ++i) CHECK(n.value[i] >= 0 && n.value[i] < 1);
}

} // namespace

void sequencesTests() {
    timesFillTheLoop();
    timesUseEveryLength();
    maxSlots();
    swingHasOneValuePerPair();
    sustainInRange();
    notesInUnitRange();
}
