#pragma once

#include "Grid.h"
#include "Rng.h"

namespace ca {

// The random sequences (docs/spec.md §3). Fixed sizes: nothing allocates.
// N = Times::count. Swing has N - 1 values, Sustain and Notes have N.

// 1 · times: slot lengths in quanta. Each a power of two in [2^minExp, 2^maxExp] beats. Sum == loop.
struct Times {
    int count = 0;
    int quanta[kMaxSlots] = {};
};

// 2 · swing: one value in [-1, 1) per neighbouring slot pair. Scaled by the Swing param at table build.
struct Swing {
    double value[kMaxSlots] = {};
};

// 3 · sustain: sounding length of each slot's note in quanta (a power of two). Unused when legato is on.
struct Sustain {
    int quanta[kMaxSlots] = {};
};

// 5 · notes: one value in [0, 1) per slot; picks a note of the held chord at play time.
struct Notes {
    double value[kMaxSlots] = {};
};

// Exponents are 2^n beats. min > max is swapped. loopExp bounds the slot lengths.
void generateTimes(Rng& rng, int loopExp, int minExp, int maxExp, Times& out);
void generateSwing(Rng& rng, int count, Swing& out);
void generateSustain(Rng& rng, int count, int minExp, int maxExp, Sustain& out);
void generateNotes(Rng& rng, int count, Notes& out);

} // namespace ca
