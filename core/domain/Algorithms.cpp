#include "Algorithms.h"

#include <cstring>

namespace ca {

namespace {

int clampIndex(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// The window: n = min(notesCount, k) chord notes from lo, lo picked by baseNote and moved down to fit.
void windowOf(const AlgorithmParams& p, int k, int& lo, int& n) {
    n = clampIndex(p.notesCount, 1, k);
    lo = clampIndex(static_cast<int>(p.baseNote * static_cast<float>(k)), 0, k - n);
}

// The middle of chord index i's band: HeldNotes::pick(value) gives back index i.
float valueOf(int index, int k) { return (static_cast<float>(index) + 0.5f) / static_cast<float>(k); }

} // namespace

void StepAlgorithm::start(const AlgorithmParams& params) {
    params_ = params;
    step_ = 0;
}

float StepAlgorithm::next(int notesCount) {
    if (notesCount < 1) notesCount = 1;
    int lo, n;
    windowOf(params_, notesCount, lo, n);
    const int i = clampIndex(index(step_++, lo, n), 0, notesCount - 1);
    return valueOf(i, notesCount);
}

int UpAlgorithm::index(int s, int lo, int n) { return lo + s % n; }

int DownAlgorithm::index(int s, int lo, int n) { return lo + n - 1 - s % n; }

int UpDownExclusiveAlgorithm::index(int s, int lo, int n) {
    if (n < 2) return lo;
    const int t = s % (2 * n - 2);
    return lo + (t < n ? t : 2 * n - 2 - t);
}

int UpDownInclusiveAlgorithm::index(int s, int lo, int n) {
    const int t = s % (2 * n);
    return lo + (t < n ? t : 2 * n - 1 - t);
}

int DownUpAlgorithm::index(int s, int lo, int n) {
    if (n < 2) return lo;
    const int t = s % (2 * n - 2);
    return lo + (t < n ? n - 1 - t : t - (n - 1));
}

int ConvergeAlgorithm::index(int s, int lo, int n) {
    const int t = s % n;
    return lo + (t % 2 == 0 ? t / 2 : n - 1 - t / 2);
}

int DivergeAlgorithm::index(int s, int lo, int n) {
    // Middle, then one below, one above, … skipping a side once it runs out.
    const int t = s % n;
    const int mid = (n - 1) / 2;
    int below = mid - 1, above = mid + 1, pos = mid;
    bool takeBelow = true;
    for (int i = 0; i < t; ++i) {
        if ((takeBelow && below >= 0) || above >= n) {
            pos = below--;
        } else {
            pos = above++;
        }
        takeBelow = !takeBelow;
    }
    return lo + pos;
}

int ThumbUpAlgorithm::index(int s, int lo, int n) {
    if (n < 2) return lo;
    return s % 2 == 0 ? lo : lo + 1 + (s / 2) % (n - 1);
}

int PinkyUpAlgorithm::index(int s, int lo, int n) {
    if (n < 2) return lo;
    return s % 2 == 0 ? lo + n - 1 : lo + (s / 2) % (n - 1);
}

int AlbertiAlgorithm::index(int s, int lo, int n) {
    static constexpr int kOrder[4] = {0, 2, 1, 2};   // low, high, middle, high
    switch (kOrder[s % 4]) {
    case 0: return lo;
    case 2: return lo + n - 1;
    default: return lo + (n - 1) / 2;
    }
}

int LeapfrogAlgorithm::index(int s, int lo, int n) { return lo + (s / 2 + (s % 2) * 2) % n; }

int StaircaseAlgorithm::index(int s, int lo, int n) { return lo + (s / 3 + s % 3) % n; }

int TrillAlgorithm::index(int s, int lo, int n) { return lo + (s / 4 + s % 2) % n; }

int RootBounceAlgorithm::index(int s, int lo, int n) {
    // The others: the window without the root (chord index 0).
    const int first = lo == 0 ? 1 : lo;
    const int others = lo + n - first;
    if (s % 2 == 0 || others <= 0) return 0;
    return first + (s / 2) % others;
}

void RandomWalkAlgorithm::start(const AlgorithmParams& params) {
    params_ = params;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &params.rng, sizeof bits);
    rng_ = Rng(0x5EED0000ull ^ bits);
    pos_ = 0;
    first_ = true;
}

float RandomWalkAlgorithm::next(int notesCount) {
    if (notesCount < 1) notesCount = 1;
    int lo, n;
    windowOf(params_, notesCount, lo, n);
    if (first_) {
        first_ = false;
    } else if (n > 1) {
        pos_ += rng_.unit() < 0.5 ? -1 : 1;
        if (pos_ < 0) pos_ = 1;
        if (pos_ > n - 1) pos_ = n - 2;
    }
    pos_ = clampIndex(pos_, 0, n - 1);
    return valueOf(lo + pos_, notesCount);
}

Algorithms::Algorithms()
    : all_{&up_,      &down_,     &upDownExclusive_, &upDownInclusive_, &downUp_,
           &converge_, &diverge_, &thumbUp_,         &pinkyUp_,         &alberti_,
           &randomWalk_, &leapfrog_, &staircase_,   &trill_,           &rootBounce_} {}

ArpegioAlgorithm& Algorithms::operator[](int index) { return *all_[clampIndex(index, 0, kAlgorithmCount - 1)]; }

} // namespace ca
