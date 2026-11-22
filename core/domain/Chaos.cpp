#include "Chaos.h"

#include <cmath>

namespace ca {

namespace {

void ends(const ChaosRange& r, double chaos, double& lo, double& hi) {
    if (chaos < 0) chaos = 0;
    if (chaos > 1) chaos = 1;
    lo = r.low0 + (r.low1 - r.low0) * chaos;
    hi = r.high0 + (r.high1 - r.high0) * chaos;
    if (lo > hi) {
        double t = lo;
        lo = hi;
        hi = t;
    }
}

} // namespace

double chaosValue(const ChaosRange& range, double chaos, double roll) {
    double lo, hi;
    ends(range, chaos, lo, hi);
    return lo + (hi - lo) * roll;
}

int chaosStep(const ChaosRange& range, double chaos, double roll) {
    double lo, hi;
    ends(range, chaos, lo, hi);
    const int a = static_cast<int>(std::lround(lo));
    const int b = static_cast<int>(std::lround(hi));
    const int steps = b - a + 1;
    int k = static_cast<int>(roll * steps);
    if (k >= steps) k = steps - 1;
    if (k < 0) k = 0;
    return a + k;
}

bool ChaosClock::tick(double beat, double unitBeats) {
    if (!(unitBeats > 0)) {
        unit_ = 0;   // off: the next unit size starts a fresh count
        return false;
    }
    if (unitBeats != unit_) {
        // New unit size: count again from the origin without a roll.
        unit_ = unitBeats;
        index_ = static_cast<long long>(std::floor((beat - origin_) / unit_ + 1e-9));
        return false;
    }
    const long long index = static_cast<long long>(std::floor((beat - origin_) / unit_ + 1e-9));
    if (index > index_) {
        index_ = index;
        return true;
    }
    index_ = index;   // a jump back keeps counting from there
    return false;
}

} // namespace ca
