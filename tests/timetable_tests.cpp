#include "Check.h"
#include "TimeTable.h"

using namespace ca;

namespace {

// Slots of 1, 2, 1 beats (loop 4).
Times threeSlots() {
    Times t;
    t.count = 3;
    t.quanta[0] = 64;
    t.quanta[1] = 128;
    t.quanta[2] = 64;
    return t;
}

void legatoHasNoSilence() {
    Times t = threeSlots();
    Swing s;
    Sustain sus;
    TimeTable table;
    buildTimeTable(t, s, 0, sus, true, table);
    CHECK_EQ(table.loopQuanta, 256);
    for (int q = 0; q < 256; ++q) {
        const int expected = q < 64 ? 0 : (q < 192 ? 1 : 2);
        CHECK_EQ(slotAt(table, q), expected);
    }
    // Cells as big as the table allows: gcd of 64, 192, 256.
    CHECK_EQ(table.cellQuanta, 64);
}

void sustainLeavesSilence() {
    Times t = threeSlots();
    Swing s;
    Sustain sus;
    sus.quanta[0] = 16;    // 1/4 beat
    sus.quanta[1] = 256;   // longer than its slot: cut
    sus.quanta[2] = 2;     // 1/32 beat
    TimeTable table;
    buildTimeTable(t, s, 0, sus, false, table);
    CHECK_EQ(slotAt(table, 0), 0);
    CHECK_EQ(slotAt(table, 15), 0);
    CHECK_EQ(slotAt(table, 16), static_cast<int>(kSilence));
    CHECK_EQ(slotAt(table, 63), static_cast<int>(kSilence));
    CHECK_EQ(slotAt(table, 64), 1);
    CHECK_EQ(slotAt(table, 191), 1);
    CHECK_EQ(table.soundEnd[1], 192);
    CHECK_EQ(slotAt(table, 192), 2);
    CHECK_EQ(slotAt(table, 193), 2);
    CHECK_EQ(slotAt(table, 194), static_cast<int>(kSilence));
    CHECK_EQ(table.cellQuanta, 2);
}

void swingMovesBorders() {
    Times t = threeSlots();
    Sustain sus;
    TimeTable table;

    // +1: slot 0 grows into slot 1 by half of slot 1 (64 quanta).
    Swing grow;
    grow.value[0] = 1;
    buildTimeTable(t, grow, 1, sus, true, table);
    CHECK_EQ(table.border[1], 64 + 64);

    // -1 on the second pair: slot 2 starts early by half of slot 1.
    Swing early;
    early.value[1] = -1;
    buildTimeTable(t, early, 1, sus, true, table);
    CHECK_EQ(table.border[2], 192 - 64);

    // The Swing param scales it.
    buildTimeTable(t, grow, 0.5, sus, true, table);
    CHECK_EQ(table.border[1], 64 + 32);

    // Swing 0: no move.
    buildTimeTable(t, grow, 0, sus, true, table);
    CHECK_EQ(table.border[1], 64);
}

void everySlotKeepsAQuantum() {
    Times t;
    t.count = 4;
    for (int i = 0; i < 4; ++i) t.quanta[i] = 4;
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
    CHECK_EQ(slotAt(table, 256 + 70), 1);
    CHECK_EQ(slotAt(table, -1), 2);
    CHECK_EQ(slotStartAt(table, 256 + 70), 256 + 64);
    CHECK_EQ(slotStartAt(table, -1), -64);

    // One slot: each loop pass is a new slot start.
    Times one;
    one.count = 1;
    one.quanta[0] = 64;
    buildTimeTable(one, s, 0, sus, true, table);
    CHECK_EQ(slotStartAt(table, 10), 0);
    CHECK_EQ(slotStartAt(table, 70), 64);
}

} // namespace

void timeTableTests() {
    legatoHasNoSilence();
    sustainLeavesSilence();
    swingMovesBorders();
    everySlotKeepsAQuantum();
    wrapsAndSlotStarts();
}
