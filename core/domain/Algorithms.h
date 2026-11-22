#pragma once

#include "Rng.h"

namespace ca {

// Arpeggio algorithms (docs/spec.md §3.5). Hidden from the user: the pattern sequence picks one per pattern.
//
// An algorithm walks a window of `notesCount` neighbouring notes of the held chord, starting at the chord note
// `baseNote` picks. next(k) gets the held chord size k and returns a note value in [0, 1]: the middle of the
// chosen note's band, so HeldNotes::pick(value) plays exactly that note. The window shrinks to the chord when
// the chord has fewer notes, and slides down so it stays inside the chord.

struct AlgorithmParams {
    float baseNote = 0;     // [0, 1): picks the window's lowest note
    int notesCount = 1;     // window size, 1..8
    float rng = 0;          // [0, 1): seed for algorithms that roll (Random Walk)
};

class ArpegioAlgorithm {
public:
    virtual void start(const AlgorithmParams& params) = 0;
    virtual float next(int notesCount) = 0;   // notesCount = held chord size; value in [0, 1]

protected:
    ~ArpegioAlgorithm() = default;
};

// Most algorithms are a fixed order of window positions. They give the chord index for step s.
class StepAlgorithm : public ArpegioAlgorithm {
public:
    void start(const AlgorithmParams& params) override;
    float next(int notesCount) override;

protected:
    ~StepAlgorithm() = default;
    // lo = window's lowest chord index, n = window size (>= 1). Returns a chord index.
    virtual int index(int step, int lo, int n) = 0;

    AlgorithmParams params_;
    int step_ = 0;
};

#define CA_STEP_ALGORITHM(Name)                                  \
    class Name final : public StepAlgorithm {                    \
    protected:                                                   \
        int index(int step, int lo, int n) override;             \
    };

CA_STEP_ALGORITHM(UpAlgorithm)              // C E G C E G
CA_STEP_ALGORITHM(DownAlgorithm)            // G E C G E C
CA_STEP_ALGORITHM(UpDownExclusiveAlgorithm) // C E G E C E G: ends not repeated
CA_STEP_ALGORITHM(UpDownInclusiveAlgorithm) // C E G G E C C E G: ends twice
CA_STEP_ALGORITHM(DownUpAlgorithm)          // G E C E G E C
CA_STEP_ALGORITHM(ConvergeAlgorithm)        // C G E: outside notes, moving inward
CA_STEP_ALGORITHM(DivergeAlgorithm)         // E C G: middle, moving outward
CA_STEP_ALGORITHM(ThumbUpAlgorithm)         // C E C G: the lowest note between the others
CA_STEP_ALGORITHM(PinkyUpAlgorithm)         // G C G E: the highest note between the others
CA_STEP_ALGORITHM(AlbertiAlgorithm)         // C G E G: low, high, middle, high
CA_STEP_ALGORITHM(LeapfrogAlgorithm)        // 1 3 2 4 3 5: two forward, one back
CA_STEP_ALGORITHM(StaircaseAlgorithm)       // 1 2 3, 2 3 4, 3 4 5
CA_STEP_ALGORITHM(TrillAlgorithm)           // 1 2 1 2, 2 3 2 3
CA_STEP_ALGORITHM(RootBounceAlgorithm)      // 1 2 1 3 1 4: back to the chord's root every other note

#undef CA_STEP_ALGORITHM

// Each step moves one position up or down (reflects at the window's ends).
class RandomWalkAlgorithm final : public ArpegioAlgorithm {
public:
    void start(const AlgorithmParams& params) override;
    float next(int notesCount) override;

private:
    AlgorithmParams params_;
    Rng rng_{0};
    int pos_ = 0;
    bool first_ = true;
};

inline constexpr int kAlgorithmCount = 15;

// One instance of every algorithm, made with the engine. No allocation.
class Algorithms {
public:
    Algorithms();
    Algorithms(const Algorithms&) = delete;
    Algorithms& operator=(const Algorithms&) = delete;

    // index is clamped to 0..kAlgorithmCount-1.
    ArpegioAlgorithm& operator[](int index);

private:
    UpAlgorithm up_;
    DownAlgorithm down_;
    UpDownExclusiveAlgorithm upDownExclusive_;
    UpDownInclusiveAlgorithm upDownInclusive_;
    DownUpAlgorithm downUp_;
    ConvergeAlgorithm converge_;
    DivergeAlgorithm diverge_;
    ThumbUpAlgorithm thumbUp_;
    PinkyUpAlgorithm pinkyUp_;
    AlbertiAlgorithm alberti_;
    RandomWalkAlgorithm randomWalk_;
    LeapfrogAlgorithm leapfrog_;
    StaircaseAlgorithm staircase_;
    TrillAlgorithm trill_;
    RootBounceAlgorithm rootBounce_;
    ArpegioAlgorithm* all_[kAlgorithmCount];
};

} // namespace ca
