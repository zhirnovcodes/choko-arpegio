#pragma once
#include <cstdint>

namespace ca {

// SplitMix64. Small, fast, and the same sequence on every platform for a given seed.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    std::uint32_t next32() { return static_cast<std::uint32_t>(next() >> 32); }

    // [0, 1)
    double unit() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }

    // [-1, 1)
    double signedUnit() { return unit() * 2.0 - 1.0; }

    // Integer in [lo, hi], both inclusive. lo <= hi.
    int range(int lo, int hi) {
        const std::uint64_t span = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<int>(next() % span);
    }

private:
    std::uint64_t state_;
};

} // namespace ca
