#pragma once

#include "Chaos.h"

namespace ca {

// The range of every param a chaos roll changes: {low0, high0, low1, high1} = [low, high] at chaos 0
// and at chaos 1. The chaos value blends each end between them (docs/spec.md §5). Tune them here.
// Pow2 params are exponents: n means 2^n beats (2^n loops for chaos loop).
// A roll is clamped to its param's limits, so a range may reach past them. A min above its max is swapped.
namespace chaosRanges {

// 2^n beats, -4..4. Chaos 0: 1/4..8 beats. Chaos 1: 1/16..1 beat.
constexpr ChaosRange noteLengthMin{-2, 3, -4, 0};
constexpr ChaosRange noteLengthMax{-2, 3, -4, 0};

// 2^n beats, -5..4. Chaos 0: 1/4..8 beats. Chaos 1: 1/32..16 beats.
constexpr ChaosRange sustainMin{-2, 3, -5, 4};
constexpr ChaosRange sustainMax{-2, 3, -5, 4};

// 0..1.
constexpr ChaosRange swing{0, 0.2, 0, 1};

// 2^n loops, -3..3. Chaos 0: 1..8 loops. Chaos 1: 1/8..1 loop.
constexpr ChaosRange chaosLoop{0, 3, -3, 0};

// 2^n beats, -3..5. Chaos 0: 2..32 beats. Chaos 1: 1/8..2 beats.
constexpr ChaosRange chaosBeat{1, 5, -3, 1};

} // namespace chaosRanges

} // namespace ca
