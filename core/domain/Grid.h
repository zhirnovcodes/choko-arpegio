#pragma once

namespace ca {

// All musical time inside the core is in quanta: 1/64 beat. Every length the params allow
// (2^-5 beats and up) and every swing shift lands on this grid. See docs/spec.md §2.
inline constexpr int kQuantaPerBeat = 64;
inline constexpr int kQuantaExp = 6;                       // 2^6 = 64

// Param exponent limits (params.json). 2^n beats.
inline constexpr int kLoopExpMax = 5;                      // 32 beats
inline constexpr int kNoteExpMin = -4;                     // 1/16 beat
inline constexpr int kSustainExpMin = -5;                  // 1/32 beat

inline constexpr int kMaxLoopQuanta = (1 << kLoopExpMax) * kQuantaPerBeat;          // 2048
inline constexpr int kMaxSlots = (1 << (kLoopExpMax - kNoteExpMin));                // 512

// 2^e beats in quanta. Exponents below the grid round up to one quantum.
inline constexpr int quantaOfExp(int e) { return e >= -kQuantaExp ? (1 << (e + kQuantaExp)) : 1; }

inline constexpr int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace ca
