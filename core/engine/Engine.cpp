#include "Engine.h"

#include <cmath>
#include <cstring>

#include "ParamsJson.h"   // written by CMake from params.json and chaos-ranges.json

namespace ca {

static_assert(sizeof(kParams) / sizeof(kParams[0]) == kParamCount, "params.json and enum Param differ in length");

namespace {

bool rounds(const ParamSpec& p) { return p.kind[0] == 'i' || p.kind[0] == 'c' || p.kind[0] == 'b' || p.kind[0] == 't'; }
bool isTrigger(const ParamSpec& p) { return p.kind[0] == 't'; }

constexpr double kEps = 1e-7;
constexpr double kJumpBeats = 1e-3;

} // namespace

Engine::Engine(double sampleRate, std::uint32_t seed)
    : sampleRate_(sampleRate > 0 ? sampleRate : 48000), master_(seed), chaosRng_(seed ^ 0xC4A05C4A05ull) {
    for (int i = 0; i < kParamCount; ++i) params_[i] = kParams[i].def;

    // Chaos ranges by param id. Pow2 params: beats (or loops) in the file, exponents here.
    for (const ChaosRangeSpec& spec : kChaosRanges) {
        for (int i = 0; i < kParamCount; ++i) {
            if (std::strcmp(spec.id, kParams[i].id) != 0) continue;
            ChaosRange r{spec.low0, spec.high0, spec.low1, spec.high1};
            if (kParams[i].pow2) {
                r = {std::log2(spec.low0), std::log2(spec.high0), std::log2(spec.low1), std::log2(spec.high1)};
            }
            chaosRange_[i] = r;
            hasChaosRange_[i] = true;
        }
    }

    reseed(kDirtyAll);
    rebuild();
}

int Engine::intParam(int index) const { return static_cast<int>(std::lround(params_[index])); }

double Engine::param(int index) const {
    if (index < 0 || index >= kParamCount) return 0;
    return params_[index];
}

void Engine::setParam(int index, double value) {
    if (index < 0 || index >= kParamCount || !std::isfinite(value)) return;
    if (isTrigger(kParams[index])) {
        // Fires once per rise past 0.5, reads back 0.
        const bool high = value > 0.5;
        if (high && !triggerHigh_[index]) {
            if (index == kShuffle) reseed(kDirtyAll);
            if (index == kChaos) rollChaos();
        }
        triggerHigh_[index] = high;
        return;
    }
    store(index, value);
}

void Engine::store(int index, double value) {
    const ParamSpec& spec = kParams[index];
    if (value < spec.min) value = spec.min;
    if (value > spec.max) value = spec.max;
    if (rounds(spec)) value = std::round(value);
    if (params_[index] == value) return;
    params_[index] = value;

    // What each param regenerates: docs/spec.md §6.1.
    switch (index) {
    case kLoop:
    case kNoteLengthMin:
    case kNoteLengthMax:
        reseed(kDirtyAll);
        break;
    case kLegato:
    case kSustainMin:
    case kSustainMax:
        reseed(kDirtySustain);
        break;
    case kSwing:
        dirty_ |= kDirtyTable;
        break;
    case kRetrig:
        slotStart_ = kNoSlot;   // the time origin moved
        break;
    default:
        break;
    }
}

void Engine::reseed(unsigned mask) {
    if (mask & kDirtyTimes) seeds_[kSeedTimes] = master_.next32();
    if (mask & kDirtySwing) seeds_[kSeedSwing] = master_.next32();
    if (mask & kDirtySustain) seeds_[kSeedSustain] = master_.next32();
    if (mask & kDirtyNotes) seeds_[kSeedNotes] = master_.next32();
    dirty_ |= mask;
}

std::uint32_t Engine::seed(int which) const { return which >= 0 && which < kSeedCount ? seeds_[which] : 0; }

void Engine::setSeed(int which, std::uint32_t value) {
    static constexpr unsigned kBits[kSeedCount] = {kDirtyTimes, kDirtySwing, kDirtySustain, kDirtyNotes};
    if (which < 0 || which >= kSeedCount) return;
    seeds_[which] = value;
    dirty_ |= kBits[which];
}

void Engine::rebuild() {
    if (dirty_ == 0) return;
    // Times sets the slot count, so every sequence follows, each from its own seed.
    if (dirty_ & kDirtyTimes) dirty_ |= kDirtySwing | kDirtySustain | kDirtyNotes;
    if (dirty_ & (kDirtySwing | kDirtySustain)) dirty_ |= kDirtyTable;

    if (dirty_ & kDirtyTimes) {
        Rng r(seeds_[kSeedTimes]);
        generateTimes(r, intParam(kLoop), intParam(kNoteLengthMin), intParam(kNoteLengthMax), times_);
    }
    if (dirty_ & kDirtySwing) {
        Rng r(seeds_[kSeedSwing]);
        generateSwing(r, times_.count, swing_);
    }
    if (dirty_ & kDirtySustain) {
        Rng r(seeds_[kSeedSustain]);
        generateSustain(r, times_.count, intParam(kSustainMin), intParam(kSustainMax), sustain_);
    }
    if (dirty_ & kDirtyNotes) {
        Rng r(seeds_[kSeedNotes]);
        generateNotes(r, times_.count, notes_);
    }
    if (dirty_ & kDirtyTable) buildTimeTable(times_, swing_, params_[kSwing], sustain_, intParam(kLegato) != 0, table_);
    dirty_ = 0;
}

// ---- chaos ------------------------------------------------------------------------------------

void Engine::rollChaos() {
    const double chaos = params_[kChaosValue];
    auto roll = [&](int index) {
        if (!hasChaosRange_[index]) return;
        const ParamSpec& spec = kParams[index];
        const double v = rounds(spec) ? chaosStep(chaosRange_[index], chaos, chaosRng_.unit())
                                      : chaosValue(chaosRange_[index], chaos, chaosRng_.unit());
        store(index, v);
        chaosChanged_ |= 1u << index;
    };
    auto order = [&](int lo, int hi) {
        if (params_[lo] > params_[hi]) {
            const double t = params_[lo];
            store(lo, params_[hi]);
            store(hi, t);
        }
    };
    const auto on = [&](int index) { return intParam(index) != 0; };

    if (on(kChaosShuffle)) reseed(kDirtyAll);
    if (on(kChaosLength)) {
        roll(kNoteLengthMin);
        roll(kNoteLengthMax);
        order(kNoteLengthMin, kNoteLengthMax);
    }
    if (on(kChaosSwing)) roll(kSwing);
    if (on(kChaosSustain)) {
        roll(kSustainMin);
        roll(kSustainMax);
        order(kSustainMin, kSustainMax);
    }
    if (on(kChaosChaos)) {
        roll(kChaosLoop);
        roll(kChaosBeat);
    }
}

std::uint32_t Engine::takeChaosChanges() {
    const std::uint32_t m = chaosChanged_;
    chaosChanged_ = 0;
    return m;
}

// ---- view -------------------------------------------------------------------------------------

int Engine::slotCount() {
    rebuild();
    return table_.slotCount;
}

double Engine::slotStart(int slot) {
    rebuild();
    if (slot < 0 || slot >= table_.slotCount) return 0;
    return table_.border[slot] / static_cast<double>(kQuantaPerBeat);
}

double Engine::slotEnd(int slot) {
    rebuild();
    if (slot < 0 || slot >= table_.slotCount) return 0;
    return table_.border[slot + 1] / static_cast<double>(kQuantaPerBeat);
}

double Engine::soundEnd(int slot) {
    rebuild();
    if (slot < 0 || slot >= table_.slotCount) return 0;
    return table_.soundEnd[slot] / static_cast<double>(kQuantaPerBeat);
}

double Engine::noteValue(int slot) {
    rebuild();
    if (slot < 0 || slot >= table_.slotCount) return 0;
    return notes_.value[slot];
}

// ---- time -------------------------------------------------------------------------------------

double Engine::beatAtAbs(long long absFrame) const {
    return anchorBeat_ + static_cast<double>(absFrame - anchorFrame_) * bpm_ / (60.0 * sampleRate_);
}

long long Engine::absFrameOfBeat(double beat) const {
    const double frames = (beat - anchorBeat_) * 60.0 * sampleRate_ / bpm_;
    return anchorFrame_ + static_cast<long long>(std::ceil(frames - kEps));
}

// Retrig: time counts from the last note start. Otherwise the DAW's beat.
double Engine::origin() const { return intParam(kRetrig) != 0 ? retrigOrigin_ : 0.0; }

double Engine::loopBeats() const { return quantaOfExp(intParam(kLoop)) / static_cast<double>(kQuantaPerBeat); }

long long Engine::quantaAt(double beat) const {
    return static_cast<long long>(std::floor((beat - origin()) * kQuantaPerBeat + kEps));
}

// ---- processing -------------------------------------------------------------------------------

void Engine::transport(const Transport& t, int frame) {
    if (pendingCount_ >= kMaxPending) return;
    Pending& p = pending_[pendingCount_++];
    p.frame = frame;
    p.isTransport = true;
    p.t = t;
}

void Engine::midi(int frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) {
    if (pendingCount_ >= kMaxPending) return;
    Pending& p = pending_[pendingCount_++];
    p.frame = frame;
    p.isTransport = false;
    p.status = status;
    p.d1 = d1;
    p.d2 = d2;
}

void Engine::process(int frames, float* left, float* right, OutBuffer& out) {
    (void)left;
    (void)right;
    if (frames < 0) frames = 0;
    rebuild();
    int cursor = 0;
    bool notesChanged = false;
    for (int i = 0; i < pendingCount_; ++i) {
        int f = pending_[i].frame;
        if (f < cursor) f = cursor;
        if (f > frames) f = frames;
        if (f > cursor) {
            flushNotes(cursor, notesChanged, out);
            renderSegment(cursor, f, out);
        }
        pending_[i].frame = f;
        if (pending_[i].isTransport) {
            flushNotes(f, notesChanged, out);
            applyPending(pending_[i], out);
        } else {
            notesChanged = applyNote(f, pending_[i].status, pending_[i].d1, pending_[i].d2, out) || notesChanged;
        }
        cursor = f;
    }
    pendingCount_ = 0;
    flushNotes(cursor, notesChanged, out);
    renderSegment(cursor, frames, out);
    blockStart_ += frames;
}

// All input notes at one frame are one chord: the note is picked once, after all of them.
void Engine::flushNotes(int frame, bool& notesChanged, OutBuffer& out) {
    if (!notesChanged) return;
    notesChanged = false;
    if (playing_) play(frame, quantaAt(beatAtAbs(blockStart_ + frame)), out);
}

void Engine::applyPending(const Pending& p, OutBuffer& out) {
    const Transport& t = p.t;
    const long long abs = blockStart_ + p.frame;
    const double expected = beatAtAbs(abs);
    const bool wasPlaying = playing_;
    const double beat = std::isfinite(t.beat) ? t.beat : 0;

    if (t.bpm > 0 && std::isfinite(t.bpm)) bpm_ = t.bpm;
    anchorFrame_ = abs;
    anchorBeat_ = beat;

    if (!t.playing) {
        playing_ = false;
        stopNote(p.frame, out);
        slotStart_ = kNoSlot;
        return;
    }
    playing_ = true;
    const bool jump = !wasPlaying || std::fabs(beat - expected) > kJumpBeats;
    if (!jump) return;   // tempo change or a steady update: the grid carries on

    // Start or seek: stop what sounds, count chaos from here, play what is under the playhead.
    stopNote(p.frame, out);
    slotStart_ = kNoSlot;
    if (!wasPlaying && !held_.empty()) retrigOrigin_ = beat;
    chaosClock_.restart(beat);
    play(p.frame, quantaAt(beat), out);
}

// Updates the held chord. True when it changed and the note must be re-checked (see flushNotes).
bool Engine::applyNote(int frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2, OutBuffer& out) {
    const int type = status & 0xF0;
    if (type == 0xB0) {
        if (d1 == 120 || d1 == 123) {
            stopNote(frame, out);
            held_.clear();
            slotStart_ = kNoSlot;
        }
        return false;
    }
    const double beat = beatAtAbs(blockStart_ + frame);
    if (type == 0x90 && d2 > 0) {
        const bool fromSilence = held_.empty();
        held_.noteOn(d1, d2);
        if (fromSilence) {
            // A new chord: retrig counts from here, so does chaos; Note trigger rolls.
            retrigOrigin_ = beat;
            slotStart_ = kNoSlot;
            chaosClock_.restart(beat);
            if (intParam(kChaosTrigger) == static_cast<int>(ChaosTrigger::Note)) rollChaos();
        }
    } else if (type == 0x80 || type == 0x90) {
        held_.noteOff(d1);
    } else {
        return false;
    }
    return true;
}

// Notes change only on the 1/64 beat grid (from the time origin): every slot border and sound end is on it.
void Engine::renderSegment(int from, int to, OutBuffer& out) {
    if (!playing_ || to <= from) return;
    const long long absFrom = blockStart_ + from;
    const long long absTo = blockStart_ + to;
    const double o = origin();
    long long k = static_cast<long long>(std::floor((beatAtAbs(absFrom) - o) * kQuantaPerBeat)) - 1;
    for (;; ++k) {
        const long long f = absFrameOfBeat(o + static_cast<double>(k) / kQuantaPerBeat);
        if (f >= absTo) break;
        if (f >= absFrom) onGrid(static_cast<int>(f - blockStart_), k, out);
    }
}

void Engine::onGrid(int frame, long long quanta, OutBuffer& out) {
    const double beat = origin() + static_cast<double>(quanta) / kQuantaPerBeat;
    double unit = 0;
    switch (static_cast<ChaosTrigger>(intParam(kChaosTrigger))) {
    case ChaosTrigger::Beat:
        unit = quantaOfExp(intParam(kChaosBeat)) / static_cast<double>(kQuantaPerBeat);
        break;
    case ChaosTrigger::Loop:
        unit = std::ldexp(loopBeats(), intParam(kChaosLoop));
        break;
    default:
        break;
    }
    if (chaosClock_.tick(beat, unit)) rollChaos();
    play(frame, quanta, out);
}

// Spec §4: slot → note, latched at the slot start (§4.1), restarted on every new slot.
void Engine::play(int frame, long long quanta, OutBuffer& out) {
    if (!playing_ || held_.empty()) {
        stopNote(frame, out);
        slotStart_ = kNoSlot;
        return;
    }
    rebuild();
    const int slot = slotAt(table_, quanta);
    if (slot == kSilence) {
        stopNote(frame, out);
        return;
    }
    const long long start = slotStartAt(table_, quanta);
    if (start != slotStart_) {
        slotStart_ = start;
        latchedVersion_ = held_.version();
        stopNote(frame, out);
        startNote(frame, held_.pick(notes_.value[slot]), out);
        return;
    }
    if (held_.version() != latchedVersion_) {
        latchedVersion_ = held_.version();
        if (quanta - start < kGraceQuanta) {
            const int pitch = held_.pick(notes_.value[slot]);
            if (pitch != pitch_) {
                stopNote(frame, out);
                startNote(frame, pitch, out);
            }
        }
    }
}

void Engine::startNote(int frame, int pitch, OutBuffer& out) {
    if (pitch < 0 || pitch > 127) return;
    int velocity = held_.velocity(pitch);
    if (velocity < 1) velocity = 1;
    out.push({frame, OutEvent::Midi, 0x90, static_cast<std::uint8_t>(pitch), static_cast<std::uint8_t>(velocity), 0, 0});
    pitch_ = pitch;
}

void Engine::stopNote(int frame, OutBuffer& out) {
    if (pitch_ < 0) return;
    out.push({frame, OutEvent::Midi, 0x80, static_cast<std::uint8_t>(pitch_), 0, 0, 0});
    pitch_ = -1;
}

void Engine::allOff(int frame, OutBuffer& out) {
    pendingCount_ = 0;
    stopNote(frame, out);
    held_.clear();
    slotStart_ = kNoSlot;
}

} // namespace ca
