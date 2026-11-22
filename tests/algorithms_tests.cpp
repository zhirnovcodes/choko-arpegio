#include <cstdlib>
#include <vector>

#include "Algorithms.h"
#include "Check.h"

using namespace ca;

namespace {

// Runs `steps` notes over a chord of k notes; returns chord indexes, as HeldNotes::pick reads them.
std::vector<int> run(ArpegioAlgorithm& a, float base, int notesCount, int k, int steps, float rng = 0.3f) {
    a.start({base, notesCount, rng});
    std::vector<int> out;
    for (int i = 0; i < steps; ++i) {
        const float v = a.next(k);
        CHECK(v >= 0 && v <= 1);
        int index = static_cast<int>(v * static_cast<float>(k));
        if (index >= k) index = k - 1;
        out.push_back(index);
    }
    return out;
}

void expect(int algorithm, float base, int notesCount, int k, std::vector<int> want) {
    Algorithms all;
    const std::vector<int> got = run(all[algorithm], base, notesCount, k, static_cast<int>(want.size()));
    for (size_t i = 0; i < want.size(); ++i) {
        if (got[i] != want[i]) {
            std::printf("  algorithm %d step %d\n", algorithm, static_cast<int>(i));
            CHECK_EQ(got[i], want[i]);
            return;
        }
    }
}

// Index order matches Algorithms: 0 Up … 14 Root Bounce. C E G = chord indexes 0 1 2.
void patternsOverATriad() {
    expect(0, 0, 3, 3, {0, 1, 2, 0, 1, 2});            // Up
    expect(1, 0, 3, 3, {2, 1, 0, 2, 1, 0});            // Down
    expect(2, 0, 3, 3, {0, 1, 2, 1, 0, 1, 2});         // Up-Down exclusive
    expect(3, 0, 3, 3, {0, 1, 2, 2, 1, 0, 0, 1, 2});   // Up-Down inclusive
    expect(4, 0, 3, 3, {2, 1, 0, 1, 2, 1, 0});         // Down-Up
    expect(5, 0, 3, 3, {0, 2, 1, 0, 2, 1});            // Converge
    expect(5, 0, 4, 4, {0, 3, 1, 2});
    expect(6, 0, 3, 3, {1, 0, 2, 1, 0, 2});            // Diverge
    expect(6, 0, 4, 4, {1, 0, 2, 3});
    expect(7, 0, 3, 3, {0, 1, 0, 2, 0, 1});            // Thumb Up
    expect(8, 0, 3, 3, {2, 0, 2, 1, 2, 0});            // Pinky Up
    expect(9, 0, 3, 3, {0, 2, 1, 2, 0, 2});            // Alberti
    expect(11, 0, 5, 5, {0, 2, 1, 3, 2, 4});           // Leapfrog
    expect(12, 0, 5, 5, {0, 1, 2, 1, 2, 3, 2, 3, 4});  // Staircase
    expect(13, 0, 3, 3, {0, 1, 0, 1, 1, 2, 1, 2});     // Trill
    expect(14, 0, 4, 4, {0, 1, 0, 2, 0, 3});           // Root Bounce
}

void windowFollowsBaseAndChord() {
    // Base note picks the window's lowest note; the window slides down to fit inside the chord.
    expect(0, 0.5f, 2, 6, {3, 4, 3, 4});
    expect(0, 0.99f, 3, 6, {3, 4, 5, 3});
    // Wider window than the chord: the whole chord.
    expect(0, 0.7f, 8, 3, {0, 1, 2, 0});
    // Root Bounce always returns to the chord's root, even with the window higher up.
    expect(14, 0.9f, 2, 6, {0, 4, 0, 5});
    // One note: every algorithm repeats it.
    Algorithms all;
    for (int a = 0; a < kAlgorithmCount; ++a) {
        for (int index : run(all[a], 0.4f, 1, 5, 9)) {
            if (a == 14) CHECK(index == 0 || index == 2);
            else CHECK_EQ(index, 2);
        }
    }
}

void randomWalkMovesOneStep() {
    Algorithms all;
    for (int seed = 0; seed < 20; ++seed) {
        const std::vector<int> got = run(all[10], 0, 6, 6, 64, seed / 20.0f);
        CHECK_EQ(got[0], 0);
        for (size_t i = 1; i < got.size(); ++i) CHECK_EQ(std::abs(got[i] - got[i - 1]), 1);
        // The same rng replays the same walk.
        CHECK(run(all[10], 0, 6, 6, 64, seed / 20.0f) == got);
    }
}

void everyAlgorithmStaysInTheChord() {
    Algorithms all;
    for (int a = 0; a < kAlgorithmCount; ++a) {
        for (int k = 1; k <= 8; ++k) {
            for (int n : {1, 2, 3, 4, 6, 8}) {
                for (float base : {0.0f, 0.3f, 0.999f}) {
                    for (int index : run(all[a], base, n, k, 40)) CHECK(index >= 0 && index < k);
                }
            }
        }
    }
}

} // namespace

void algorithmsTests() {
    patternsOverATriad();
    windowFollowsBaseAndChord();
    randomWalkMovesOneStep();
    everyAlgorithmStaysInTheChord();
}
