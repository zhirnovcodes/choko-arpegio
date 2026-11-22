#pragma once

#include "Grid.h"
#include "Rng.h"

namespace ca {

// The random sequences (docs/spec.md §3). Fixed sizes: nothing allocates.
// N = Times::count. Swing has N - 1 values, Sustain has N. Patterns follow the loop length.

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

// 5 · patterns: the loop is cut into patterns of `quanta` (2..8 beats, rolled once); each runs one arpeggio
// algorithm over the held chord (Algorithms.h). count = ceil(loop / quanta). A slot belongs to the pattern
// its start (before swing) falls in.
struct AlgorithmData {
    float startNote = 0;      // [0, 1): the algorithm's base note
    float randomOffset = 0;   // [-1, 1): times the RNG param, added to every note but the pattern's first
    int algorithm = 0;        // 0..kAlgorithmCount-1
    int notesCount = 1;       // 1, 2, 3, 4, 6 or 8: notes the algorithm walks over
};

inline constexpr int kPatternBeatsMin = 2;
inline constexpr int kPatternBeatsMax = 8;
inline constexpr int kMaxPatterns = (kMaxLoopQuanta / kQuantaPerBeat + kPatternBeatsMin - 1) / kPatternBeatsMin;   // 16

struct Patterns {
    int quanta = kPatternBeatsMin * kQuantaPerBeat;   // length of one pattern
    int count = 0;
    AlgorithmData data[kMaxPatterns] = {};
};

// Exponents are 2^n beats. min > max is swapped. loopExp bounds the slot lengths.
void generateTimes(Rng& rng, int loopExp, int minExp, int maxExp, Times& out);
void generateSwing(Rng& rng, int count, Swing& out);
void generateSustain(Rng& rng, int count, int minExp, int maxExp, Sustain& out);
void generatePatterns(Rng& rng, int loopQuanta, int algorithmCount, Patterns& out);

} // namespace ca
