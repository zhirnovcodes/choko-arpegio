#include "Sequences.h"

namespace ca {

void generateTimes(Rng& rng, int loopExp, int minExp, int maxExp, Times& out) {
    if (minExp > maxExp) {
        int t = minExp;
        minExp = maxExp;
        maxExp = t;
    }
    loopExp = clampInt(loopExp, -kQuantaExp, kLoopExpMax);
    minExp = clampInt(minExp, -kQuantaExp, kLoopExpMax);
    maxExp = clampInt(maxExp, -kQuantaExp, kLoopExpMax);
    const int loop = quantaOfExp(loopExp);

    out.count = 0;
    // Min longer than the loop: one slot, the whole loop.
    if (minExp >= loopExp) {
        out.quanta[out.count++] = loop;
        return;
    }
    if (maxExp > loopExp) maxExp = loopExp;

    // Left to right; each roll only from lengths that still fit. Loop and lengths are powers of two,
    // so what remains is always a multiple of the min length and the loop fills exactly.
    int remaining = loop;
    while (remaining > 0 && out.count < kMaxSlots) {
        int hi = maxExp;
        while (hi > minExp && quantaOfExp(hi) > remaining) --hi;
        const int len = quantaOfExp(rng.range(minExp, hi));
        out.quanta[out.count++] = len;
        remaining -= len;
    }
}

void generateSwing(Rng& rng, int count, Swing& out) {
    for (int i = 0; i < kMaxSlots; ++i) out.value[i] = 0;
    for (int i = 0; i + 1 < count && i < kMaxSlots; ++i) out.value[i] = rng.signedUnit();
}

void generateSustain(Rng& rng, int count, int minExp, int maxExp, Sustain& out) {
    if (minExp > maxExp) {
        int t = minExp;
        minExp = maxExp;
        maxExp = t;
    }
    for (int i = 0; i < kMaxSlots; ++i) out.quanta[i] = i < count ? quantaOfExp(rng.range(minExp, maxExp)) : 0;
}

void generateNotes(Rng& rng, int count, Notes& out) {
    for (int i = 0; i < kMaxSlots; ++i) out.value[i] = i < count ? rng.unit() : 0;
}

} // namespace ca
