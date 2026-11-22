#include "TimeTable.h"

#include <initializer_list>

namespace ca {

namespace {

int gcd(int a, int b) {
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

long long wrap(long long q, int loop) {
    long long r = q % loop;
    return r < 0 ? r + loop : r;
}

} // namespace

int swingShift(double d, int length) {
    if (d > 1) d = 1;
    if (d < -1) d = -1;
    const int limit = length / 3;   // lengths are 3 · 2^k quanta, so this is exact
    const double target = (d < 0 ? -d : d) * limit;
    // Nearest musical length up to the limit: straight 3 · 2^j or triplet 2^j quanta; 0 below half a quantum.
    int best = 0;
    double bestDistance = target;
    for (int n = 1; n <= limit; n *= 2) {
        for (int candidate : {n, 3 * n}) {
            if (candidate > limit) continue;
            const double distance = candidate > target ? candidate - target : target - candidate;
            if (distance < bestDistance) {
                bestDistance = distance;
                best = candidate;
            }
        }
    }
    return d < 0 ? -best : best;
}

void buildTimeTable(const Times& times, const Swing& swing, double swingAmount, const Sustain& sustain, bool legato,
                    TimeTable& out) {
    const int n = times.count;
    if (swingAmount < 0) swingAmount = 0;
    if (swingAmount > 1) swingAmount = 1;

    out.slotCount = n;
    out.border[0] = 0;
    for (int i = 0; i < n; ++i) out.border[i + 1] = out.border[i] + times.quanta[i];
    const int total = n > 0 ? out.border[n] : kQuantaPerBeat;
    out.loopQuanta = total;

    // Swing moves each inner border (like BananaChords drift, at most a third, on straight or triplet lengths).
    // d > 0: the first slot grows into the second by up to a third of the second.
    // d < 0: the second slot starts early by up to a third of the first.
    for (int i = 1; i < n; ++i) {
        const double d = swing.value[i - 1] * swingAmount;
        out.border[i] += swingShift(d, d >= 0 ? times.quanta[i] : times.quanta[i - 1]);
    }
    // Every slot keeps at least one quantum.
    for (int i = 1; i < n; ++i) {
        if (out.border[i] < out.border[i - 1] + 1) out.border[i] = out.border[i - 1] + 1;
    }
    for (int i = n - 1; i >= 1; --i) {
        if (out.border[i] > out.border[i + 1] - 1) out.border[i] = out.border[i + 1] - 1;
    }

    // Sustain is cut at the slot end. Legato fills the slot.
    int cell = total;
    for (int i = 0; i < n; ++i) {
        const int length = out.border[i + 1] - out.border[i];
        int sound = legato ? length : sustain.quanta[i];
        if (sound > length) sound = length;
        if (sound < 0) sound = 0;
        out.soundEnd[i] = out.border[i] + sound;
        cell = gcd(cell, out.border[i]);
        cell = gcd(cell, out.soundEnd[i]);
    }
    if (cell <= 0) cell = 1;
    out.cellQuanta = cell;
    out.cellCount = total / cell;

    int slot = 0;
    for (int c = 0; c < out.cellCount; ++c) {
        const int q = c * cell;
        while (slot + 1 < n && q >= out.border[slot + 1]) ++slot;
        out.cell[c] = (n > 0 && q < out.soundEnd[slot]) ? static_cast<std::uint16_t>(slot) : kSilence;
    }
}

int slotAt(const TimeTable& table, long long quanta) {
    if (table.cellCount <= 0) return kSilence;
    const long long pos = wrap(quanta, table.loopQuanta);
    return table.cell[pos / table.cellQuanta];
}

long long slotStartAt(const TimeTable& table, long long quanta) {
    if (table.slotCount <= 0) return quanta;
    const long long pos = wrap(quanta, table.loopQuanta);
    int slot = 0;
    while (slot + 1 < table.slotCount && pos >= table.border[slot + 1]) ++slot;
    return quanta - pos + table.border[slot];
}

} // namespace ca
