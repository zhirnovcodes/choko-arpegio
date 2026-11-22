#pragma once
#include <cstdint>

#include "Chaos.h"
#include "HeldNotes.h"
#include "Rng.h"
#include "Sequences.h"
#include "TimeTable.h"

namespace ca {

// Index = position in params.json. Append only.
enum Param : int {
    kLoop = 0,
    kNoteLengthMin,
    kNoteLengthMax,
    kLegato,
    kSustainMin,
    kSustainMax,
    kSwing,
    kRetrig,
    kShuffle,
    kChaos,
    kChaosValue,
    kChaosTrigger,
    kChaosLoop,
    kChaosBeat,
    kChaosShuffle,
    kChaosLength,
    kChaosSwing,
    kChaosSustain,
    kChaosChaos,
    kParamCount
};

// The random sequences. Each has its own seed, so a saved state replays exactly.
// The time table is built from the others and has no seed.
enum SeqSeed : int { kSeedTimes = 0, kSeedSwing, kSeedSustain, kSeedNotes, kSeedCount };

// A chord change this soon after a slot starts still re-picks the slot's note (spec §4.1): 1/32 beat.
inline constexpr int kGraceQuanta = kQuantaPerBeat / 32;

struct Transport {
    bool playing = false;
    double bpm = 120;
    double beat = 0;        // beats from project start at the frame this is applied
    int numerator = 4;
    int denominator = 4;
};

// What the engine sends out: MIDI notes.
struct OutEvent {
    int frame;              // 0..frames-1 inside the current process() call
    enum Kind : std::uint8_t { Midi, Control } kind;
    std::uint8_t b0, b1, b2;
    int slot;
    float value;
};

class OutBuffer {
public:
    static constexpr int kCapacity = 512;
    bool push(const OutEvent& e) {
        if (size_ >= kCapacity) return false;
        events_[size_++] = e;
        return true;
    }
    int size() const { return size_; }
    const OutEvent& operator[](int i) const { return events_[i]; }
    void clear() { size_ = 0; }

private:
    OutEvent events_[kCapacity];
    int size_ = 0;
};

class Engine {
public:
    Engine(double sampleRate, std::uint32_t seed);

    // value = plain (min..max). Out of range index: ignored. Triggers fire on a rise past 0.5.
    void setParam(int index, double value);
    double param(int index) const;

    // Apply at `frame` of the next process() call. Call in frame order.
    void transport(const Transport& t, int frame);
    void midi(int frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2);

    // left/right unused: MIDI only.
    void process(int frames, float* left, float* right, OutBuffer& out);

    // Note-offs for every sounding note at `frame`, queued events dropped, held input cleared.
    void allOff(int frame, OutBuffer& out);

    // Params chaos changed since the last call, one bit per Param. Hosts push these values back to
    // the host's parameter system so the UI and automation show them. Clears the mask.
    std::uint32_t takeChaosChanges();

    // Seeds (state save / load).
    std::uint32_t seed(int which) const;
    void setSeed(int which, std::uint32_t value);

    // ---- view (UI) ----
    int slotCount();
    double slotStart(int slot);      // beats, after swing
    double slotEnd(int slot);
    double soundEnd(int slot);       // the note sounds on [slotStart, soundEnd)
    double noteValue(int slot);      // [0, 1)
    int currentPitch() const { return pitch_; }
    // Goes up every time the sequences or the time table are rebuilt (params, Shuffle, chaos, seeds).
    // Hosts compare it after process() to know the timeline must be redrawn.
    std::uint32_t sequenceVersion() const { return sequenceVersion_; }
    // Beat the sequence counts from: 0, or the last note start when Retrig is on.
    double timeOrigin() const { return origin(); }

private:
    enum Dirty : unsigned {
        kDirtyTimes = 1u << 0,
        kDirtySwing = 1u << 1,
        kDirtySustain = 1u << 2,
        kDirtyNotes = 1u << 3,
        kDirtyTable = 1u << 4,
        kDirtyAll = kDirtyTimes | kDirtySwing | kDirtySustain | kDirtyNotes | kDirtyTable,
    };

    struct Pending {
        int frame;
        bool isTransport;
        Transport t;
        std::uint8_t status, d1, d2;
    };
    static constexpr int kMaxPending = 512;
    static constexpr long long kNoSlot = -(1ll << 62);

    int intParam(int index) const;
    void store(int index, double value);      // clamp, round, store; marks what to regenerate
    void reseed(unsigned mask);
    void rebuild();
    void rollChaos();

    double beatAtAbs(long long absFrame) const;
    long long absFrameOfBeat(double beat) const;
    double origin() const;
    double loopBeats() const;
    long long quantaAt(double beat) const;

    void renderSegment(int from, int to, OutBuffer& out);
    void onGrid(int frame, long long quanta, OutBuffer& out);
    void play(int frame, long long quanta, OutBuffer& out);
    void applyPending(const Pending& p, OutBuffer& out);
    bool applyNote(int frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2, OutBuffer& out);
    void flushNotes(int frame, bool& notesChanged, OutBuffer& out);
    void stopNote(int frame, OutBuffer& out);
    void startNote(int frame, int pitch, OutBuffer& out);

    double sampleRate_;
    Rng master_;
    Rng chaosRng_;
    double params_[kParamCount];
    std::uint32_t seeds_[kSeedCount];
    unsigned dirty_ = 0;
    std::uint32_t sequenceVersion_ = 0;
    bool triggerHigh_[kParamCount] = {};

    std::uint32_t chaosChanged_ = 0;
    ChaosClock chaosClock_;

    Times times_;
    Swing swing_;
    Sustain sustain_;
    Notes notes_;
    TimeTable table_;

    // Transport: beat at absolute frame f = anchorBeat_ + (f - anchorFrame_) * bpm / (60 * rate).
    // Absolute frames make the timing identical for every block size.
    bool playing_ = false;
    double bpm_ = 120;
    double anchorBeat_ = 0;
    long long anchorFrame_ = 0;
    long long blockStart_ = 0;

    Pending pending_[kMaxPending];
    int pendingCount_ = 0;

    HeldNotes held_;
    double retrigOrigin_ = 0;        // beat of the last note start from silence

    // The note being played: its pitch, the slot start it was latched in, the chord version it saw.
    int pitch_ = -1;
    long long slotStart_ = kNoSlot;
    unsigned latchedVersion_ = 0;
};

} // namespace ca
