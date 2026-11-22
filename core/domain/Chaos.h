#pragma once

namespace ca {

// A param's chaos range: [low0, high0] at chaos 0, [low1, high1] at chaos 1 (docs/spec.md §5).
// For pow2 params these are exponents. The values live in ChaosRanges.h.
struct ChaosRange {
    double low0 = 0;
    double high0 = 0;
    double low1 = 0;
    double high1 = 0;
};

// roll in [0, 1). Uniform between the two blended ends; ends swapped if reversed.
double chaosValue(const ChaosRange& range, double chaos, double roll);

// The blended ends rounded to whole steps; each step between them gets an equal share of the roll.
int chaosStep(const ChaosRange& range, double chaos, double roll);

enum class ChaosTrigger : int { None = 0, Note, Beat, Loop };

// Counts trigger units (beats or loops) since an origin, and says when a new unit starts.
// The origin itself is not a roll. Changing the unit size starts the count again.
class ChaosClock {
public:
    // Start counting from `beat`.
    void restart(double beat) {
        origin_ = beat;
        index_ = 0;
        unit_ = 0;
    }

    // True when `beat` is in a later unit than the last call. unitBeats <= 0: never.
    bool tick(double beat, double unitBeats);

private:
    double origin_ = 0;
    double unit_ = 0;
    long long index_ = 0;
};

} // namespace ca
