#pragma once
#include <cstdint>

#include "Grid.h"
#include "Sequences.h"

namespace ca {

inline constexpr std::uint16_t kSilence = 0xFFFF;

// 4 · time table: loop position (quanta) → slot index or kSilence. Built from the sequences, not random.
// Slot i occupies [border[i], border[i+1]) after swing; it sounds on [border[i], soundEnd[i]), then silence.
struct TimeTable {
    int loopQuanta = kQuantaPerBeat;
    int slotCount = 0;
    int cellQuanta = 1;
    int cellCount = 0;
    std::uint16_t cell[kMaxLoopQuanta] = {};
    int border[kMaxSlots + 1] = {};
    int soundEnd[kMaxSlots] = {};
};

// swingAmount in [0, 1] scales every swing value (the Swing param). Legato: notes fill their slots.
void buildTimeTable(const Times& times, const Swing& swing, double swingAmount, const Sustain& sustain, bool legato,
                    TimeTable& out);

// Slot under a loop position in quanta (any integer; wraps by the loop), or kSilence.
int slotAt(const TimeTable& table, long long quanta);

// Where the slot under `quanta` starts, in the same (unwrapped) quanta. Meaningless for silence.
long long slotStartAt(const TimeTable& table, long long quanta);

} // namespace ca
