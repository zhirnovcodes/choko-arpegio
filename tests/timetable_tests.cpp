#include "Check.h"
#include "TimeTable.h"

#include <initializer_list>

using namespace ca;

namespace {

constexpr int Q = kQuantaPerBeat;   // 192

// Slots of 1, 2, 1 beats (loop 4).
Times threeSlots() {
    Times t;
    t.count = 3;
    t.quanta[0] = Q;
    t.quanta[1] = 2 * Q;
    t.quanta[2] = Q;
    return t;
}

void legatoHasNoSilence() {
    Times t = threeSlots();
    Swing s;
    Sustain sus;
    TimeTable table;
    buildTimeTable(t, s, 0, sus, true, table);
    CHECK_EQ(table.loopQuanta, 4 * Q);
    for (int q = 0; q < 4 * Q; ++q) {
        const int expected = q < Q ? 0 : (q < 3 * Q ? 1 : 2);
        CHECK_EQ(slotAt(table, q), expected);
    }
    // Cells as big as the table allows: gcd of the borders and the loop.
    CHECK_EQ(table.cellQuanta, Q);
}

void sustainLeavesSilence() {
    Times t = threeSlots();
    Swing s;
    Sustain sus;
    sus.quanta[0] = Q / 4;    // 1/4 beat
    sus.quanta[1] = 4 * Q;    // longer than its slot: cut
    sus.quanta[2] = Q / 32;   // 1/32 beat
    TimeTable table;
    buildTimeTable(t, s, 0, sus, false, table);
    CHECK_EQ(slotAt(table, 0), 0);
    CHECK_EQ(slotAt(table, Q / 4 - 1), 0);
    CHECK_EQ(slotAt(table, Q / 4), static_cast<int>(kSilence));
    CHECK_EQ(slotAt(table, Q - 1), static_cast<int>(kSilence));
    CHECK_EQ(slotAt(table, Q), 1);
    CHECK_EQ(slotAt(table, 3 * Q - 1), 1);
    CHECK_EQ(table.soundEnd[1], 3 * Q);
    CHECK_EQ(slotAt(table, 3 * Q), 2);
    CHECK_EQ(slotAt(table, 3 * Q + Q / 32 - 1), 2);
    CHECK_EQ(slotAt(table, 3 * Q + Q / 32), static_cast<int>(kSilence));
    CHECK_EQ(table.cellQuanta, Q / 32);
}

void swingMovesBordersByUpToAThird() {
    Times t = threeSlots();
    Sustain sus;
    TimeTable table;

    // +1: slot 0 grows into slot 1 by a third of slot 1 (2 beats / 3 = 2/3 beat, a half-note triplet).
    Swing grow;
    grow.value[0] = 1;
    buildTimeTable(t, grow, 1, sus, true, table);
    CHECK_EQ(table.border[1], Q + 2 * Q / 3);

    // -1 on the second pair: slot 2 starts early by a third of slot 1.
    Swing early;
    early.value[1] = -1;
    buildTimeTable(t, early, 1, sus, true, table);
    CHECK_EQ(table.border[2], 3 * Q - 2 * Q / 3);

    // The Swing param scales it, and the shift snaps to the nearest straight or triplet length:
    // half of 2/3 beat = 1/3 beat (an 8th triplet) exactly.
    buildTimeTable(t, grow, 0.5, sus, true, table);
    CHECK_EQ(table.border[1], Q + Q / 3);
    // 0.7 · 2/3 = 0.467 beat → nearest is 1/2 beat (straight), between 1/3 and 2/3.
    buildTimeTable(t, grow, 0.7, sus, true, table);
    CHECK_EQ(table.border[1], Q + Q / 2);

    // Swing 0: no move.
    buildTimeTable(t, grow, 0, sus, true, table);
    CHECK_EQ(table.border[1], Q);
}

void swingShiftSnapsToMusicalLengths() {
    // One beat next to the border: limit 1/3 beat = 64 quanta.
    CHECK_EQ(swingShift(1, Q), Q / 3);
    CHECK_EQ(swingShift(-1, Q), -Q / 3);
    CHECK_EQ(swingShift(0, Q), 0);
    // Every result is a straight (3 · 2^j) or triplet (2^j) length, and never more than a third.
    for (int length : {Q / 16, Q / 4, Q, 8 * Q}) {
        for (int k = -100; k <= 100; ++k) {
            const int s = swingShift(k / 100.0, length);
            const int a = s < 0 ? -s : s;
            CHECK(a <= length / 3);
            const bool straight = a % 3 == 0 && ((a / 3) & (a / 3 - 1)) == 0;
            const bool triplet = (a & (a - 1)) == 0;
            CHECK(a == 0 || straight || triplet);
        }
    }
    // 1/16 beat next to the border: at most 1/48 beat (a 1/16-beat triplet).
    CHECK_EQ(swingShift(1, Q / 16), Q / 48);
}

void everySlotKeepsAQuantum() {
    Times t;
    t.count = 4;
    for (int i = 0; i < 4; ++i) t.quanta[i] = 3;
    Swing s;
    s.value[0] = 0.99;
    s.value[1] = -0.99;
    s.value[2] = 0.99;
    Sustain sus;
    TimeTable table;
    buildTimeTable(t, s, 1, sus, true, table);
    for (int i = 0; i < 4; ++i) CHECK(table.border[i + 1] - table.border[i] >= 1);
}

void wrapsAndSlotStarts() {
    Times t = threeSlots();
    Swing s;
    Sustain sus;
    TimeTable table;
    buildTimeTable(t, s, 0, sus, true, table);
    CHECK_EQ(slotAt(table, 4 * Q + Q + 10), 1);
    CHECK_EQ(slotAt(table, -1), 2);
    CHECK_EQ(slotStartAt(table, 4 * Q + Q + 10), 4 * Q + Q);
    CHECK_EQ(slotStartAt(table, -1), -Q);

    // One slot: each loop pass is a new slot start.
    Times one;
    one.count = 1;
    one.quanta[0] = Q;
    buildTimeTable(one, s, 0, sus, true, table);
    CHECK_EQ(slotStartAt(table, 10), 0);
    CHECK_EQ(slotStartAt(table, Q + 10), Q);
}

} // namespace

void timeTableTests() {
    legatoHasNoSilence();
    sustainLeavesSilence();
    swingMovesBordersByUpToAThird();
    swingShiftSnapsToMusicalLengths();
    everySlotKeepsAQuantum();
    wrapsAndSlotStarts();
}
