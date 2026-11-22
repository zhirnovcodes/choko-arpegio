#include <algorithm>
#include <cmath>
#include <vector>

#include "Check.h"
#include "Engine.h"

using namespace ca;

namespace {

constexpr double kRate = 48000;
constexpr double kBpm = 120;
constexpr double kFramesPerBeat = kRate * 60.0 / kBpm;   // 24000

struct Ev {
    long long frame;
    int b0, b1, b2;
    bool operator==(const Ev& o) const { return frame == o.frame && b0 == o.b0 && b1 == o.b1 && b2 == o.b2; }
    bool on() const { return (b0 & 0xF0) == 0x90 && b2 > 0; }
    bool off() const { return (b0 & 0xF0) == 0x80; }
};

// An input event at an absolute frame: MIDI, or a transport stop.
struct In {
    long long frame;
    int status, d1, d2;
    bool stop = false;
};

long long frameOf(double beat) { return static_cast<long long>(std::llround(beat * kFramesPerBeat)); }

// Plays from beat 0 for `totalFrames` in blocks of `block`, feeding `inputs` at their frames.
std::vector<Ev> render(Engine& e, long long totalFrames, int block, const std::vector<In>& inputs) {
    std::vector<Ev> events;
    OutBuffer out;
    Transport play;
    play.playing = true;
    play.bpm = kBpm;
    e.transport(play, 0);
    for (long long start = 0; start < totalFrames; start += block) {
        const int frames = static_cast<int>(std::min<long long>(block, totalFrames - start));
        for (const In& in : inputs) {
            if (in.frame < start || in.frame >= start + frames) continue;
            const int f = static_cast<int>(in.frame - start);
            if (in.stop) {
                Transport stop;
                stop.playing = false;
                stop.bpm = kBpm;
                stop.beat = in.frame / kFramesPerBeat;
                e.transport(stop, f);
            } else {
                e.midi(f, static_cast<std::uint8_t>(in.status), static_cast<std::uint8_t>(in.d1),
                       static_cast<std::uint8_t>(in.d2));
            }
        }
        out.clear();
        e.process(frames, nullptr, nullptr, out);
        for (int i = 0; i < out.size(); ++i) events.push_back({start + out[i].frame, out[i].b0, out[i].b1, out[i].b2});
    }
    return events;
}

std::vector<In> chord(long long frame, std::initializer_list<int> pitches, int velocity = 100) {
    std::vector<In> v;
    for (int p : pitches) v.push_back({frame, 0x90, p, velocity});
    return v;
}

// Every note-on is closed by a note-off; one output note at a time.
void checkBalanced(const std::vector<Ev>& events, bool mustEndSilent) {
    int sounding = -1;
    for (const Ev& ev : events) {
        if (ev.on()) {
            CHECK_EQ(sounding, -1);
            CHECK(ev.b2 >= 1 && ev.b2 <= 127);
            sounding = ev.b1;
        } else if (ev.off()) {
            CHECK_EQ(sounding, ev.b1);
            sounding = -1;
        }
    }
    if (mustEndSilent) CHECK_EQ(sounding, -1);
}

int countOns(const std::vector<Ev>& events) {
    int n = 0;
    for (const Ev& ev : events) n += ev.on() ? 1 : 0;
    return n;
}

void sameOutputForEveryBlockSize() {
    const long long total = frameOf(16);
    std::vector<In> in = chord(0, {60, 64, 67, 71});
    in.push_back({frameOf(5.3), 0x80, 64, 0});
    in.push_back({frameOf(9.01), 0x90, 50, 80});
    auto setup = [](Engine& e) {
        e.setParam(kSwing, 0.8);
        e.setParam(kNoteLengthMin, -3);
        e.setParam(kNoteLengthMax, 0);
    };
    Engine reference(kRate, 42);
    setup(reference);
    const std::vector<Ev> expected = render(reference, total, 4096, in);
    CHECK(countOns(expected) > 10);
    for (int block : {1, 64, 128, 512, 4096, 1000}) {
        Engine e(kRate, 42);
        setup(e);
        const std::vector<Ev> got = render(e, total, block, in);
        CHECK_EQ(got.size(), expected.size());
        CHECK(got == expected);
    }
    checkBalanced(expected, false);
}

void onlyHeldNotesWithTheirVelocity() {
    Engine e(kRate, 7);
    std::vector<In> in;
    in.push_back({0, 0x90, 60, 90});
    in.push_back({0, 0x90, 64, 70});
    in.push_back({0, 0x90, 67, 50});
    const std::vector<Ev> events = render(e, frameOf(8), 256, in);
    CHECK(countOns(events) > 4);
    for (const Ev& ev : events) {
        if (!ev.on()) continue;
        CHECK(ev.b1 == 60 || ev.b1 == 64 || ev.b1 == 67);
        if (ev.b1 == 60) CHECK_EQ(ev.b2, 90);
        if (ev.b1 == 64) CHECK_EQ(ev.b2, 70);
        if (ev.b1 == 67) CHECK_EQ(ev.b2, 50);
    }
    checkBalanced(events, false);
}

void noInputNoOutput() {
    Engine e(kRate, 7);
    CHECK(render(e, frameOf(4), 512, {}).empty());
}

void notesFollowTheSequence() {
    // Legato, so every slot sounds; each note-on is at a slot start with the pitch the notes value picks.
    Engine e(kRate, 21);
    e.setParam(kLegato, 1);
    const int pitches[] = {48, 55, 60, 64, 67};
    std::vector<In> in;
    for (int p : pitches) in.push_back({0, 0x90, p, 100});
    const std::vector<Ev> events = render(e, frameOf(4) - 1, 128, in);
    const int n = e.slotCount();
    CHECK_EQ(countOns(events), n);
    int k = 0;
    for (const Ev& ev : events) {
        if (!ev.on()) continue;
        CHECK_EQ(ev.frame, frameOf(e.slotStart(k)));
        const int index = std::min(4, static_cast<int>(e.noteValue(k) * 5));
        CHECK_EQ(ev.b1, pitches[index]);
        ++k;
    }
}

void sameNoteRestartsEverySlot() {
    // One held note, legato: every slot restarts it, off and on at the same frame.
    Engine e(kRate, 8);
    e.setParam(kLegato, 1);
    e.setParam(kLoop, 2);             // 4 beats
    e.setParam(kNoteLengthMin, -1);   // 1/2 beat
    e.setParam(kNoteLengthMax, -1);
    const std::vector<Ev> events = render(e, frameOf(4) - 1, 333, chord(0, {60}));
    CHECK_EQ(countOns(events), 8);
    for (int i = 1; i < 8; ++i) {
        const long long f = frameOf(i * 0.5);
        bool off = false, on = false;
        for (const Ev& ev : events) {
            if (ev.frame != f) continue;
            off = off || ev.off();
            on = on || ev.on();
        }
        CHECK(off && on);
    }
    checkBalanced(events, false);
}

void sustainSetsNoteLength() {
    Engine e(kRate, 9);
    e.setParam(kNoteLengthMin, -1);   // 1/2 beat slots
    e.setParam(kNoteLengthMax, -1);
    e.setParam(kSustainMin, -5);      // 1/32 beat notes
    e.setParam(kSustainMax, -5);
    const std::vector<Ev> events = render(e, frameOf(4), 100, chord(0, {60}));
    CHECK_EQ(countOns(events), 8);
    long long onFrame = -1;
    for (const Ev& ev : events) {
        if (ev.on()) onFrame = ev.frame;
        if (ev.off()) CHECK_EQ(ev.frame - onFrame, frameOf(1.0 / 32));
    }
}

void stopReleasesEverything() {
    Engine e(kRate, 3);
    e.setParam(kLegato, 1);
    std::vector<In> in = chord(0, {60, 63});
    const long long stopAt = 70001;
    in.push_back({stopAt, 0, 0, 0, true});
    const std::vector<Ev> events = render(e, frameOf(8), 128, in);
    checkBalanced(events, true);
    bool sawOffAtStop = false;
    for (const Ev& ev : events) {
        CHECK(ev.frame <= stopAt);
        sawOffAtStop = sawOffAtStop || (ev.frame == stopAt && ev.off());
    }
    CHECK(sawOffAtStop);
}

void releasingTheChordStopsTheNote() {
    Engine e(kRate, 3);
    e.setParam(kLegato, 1);
    std::vector<In> in = chord(0, {60, 63});
    const long long releaseAt = frameOf(2.3);
    in.push_back({releaseAt, 0x80, 60, 0});
    in.push_back({releaseAt, 0x90, 63, 0});   // velocity 0 = off
    const std::vector<Ev> events = render(e, frameOf(6), 64, in);
    checkBalanced(events, true);
    CHECK(!events.empty());
    CHECK_EQ(events.back().frame, releaseAt);
}

void allNotesOffCc() {
    Engine e(kRate, 3);
    e.setParam(kLegato, 1);
    std::vector<In> in = chord(0, {60});
    in.push_back({frameOf(1.1), 0xB0, 123, 0});
    const std::vector<Ev> events = render(e, frameOf(4), 256, in);
    checkBalanced(events, true);
    CHECK_EQ(events.back().frame, frameOf(1.1));
}

void chordChangeWaitsForTheNextSlot() {
    // 1-beat slots, legato. Hold 60; swap it for 50 either just after a slot start (inside the 1/32 beat
    // grace) or mid-slot. Grace: 50 plays at once. Mid-slot: 60 keeps playing until the next slot.
    for (int late = 0; late < 2; ++late) {
        Engine e(kRate, 12);
        e.setParam(kLegato, 1);
        e.setParam(kNoteLengthMin, 0);
        e.setParam(kNoteLengthMax, 0);
        const double changeBeat = late ? 1.5 : 1.01;
        std::vector<In> in = chord(0, {60});
        in.push_back({frameOf(changeBeat), 0x90, 50, 100});
        in.push_back({frameOf(changeBeat), 0x80, 60, 0});
        const std::vector<Ev> events = render(e, frameOf(3) - 1, 128, in);
        checkBalanced(events, false);
        long long first50 = -1;
        for (const Ev& ev : events) {
            if (ev.on() && ev.b1 == 50 && first50 < 0) first50 = ev.frame;
        }
        CHECK_EQ(first50, late ? frameOf(2) : frameOf(changeBeat));
    }
}

void newChordFromSilenceStartsAtOnce() {
    Engine e(kRate, 13);
    e.setParam(kLegato, 1);
    const long long at = frameOf(1.37);
    const std::vector<Ev> events = render(e, frameOf(2), 64, chord(at, {60}));
    CHECK(!events.empty());
    CHECK(events[0].on());
    CHECK_EQ(events[0].frame, at);
}

void retrigStartsTheSequenceAtTheNote() {
    // Retrig: the sequence starts at the note, so slot 0's note sounds first and slot borders follow it.
    Engine e(kRate, 14);
    e.setParam(kRetrig, 1);
    e.setParam(kLegato, 1);
    const double startBeat = 1.3;
    const int pitches[] = {40, 50, 60, 70};
    std::vector<In> in;
    for (int p : pitches) in.push_back({frameOf(startBeat), 0x90, p, 100});
    const std::vector<Ev> events = render(e, frameOf(startBeat + 4) - 1, 64, in);
    CHECK_EQ(countOns(events), e.slotCount());
    int k = 0;
    for (const Ev& ev : events) {
        if (!ev.on()) continue;
        CHECK(std::llabs(ev.frame - frameOf(startBeat + e.slotStart(k))) <= 1);
        CHECK_EQ(ev.b1, pitches[std::min(3, static_cast<int>(e.noteValue(k) * 4))]);
        ++k;
    }
}

void seedsReplay() {
    Engine a(kRate, 11);
    a.setParam(kNoteLengthMin, -4);
    a.setParam(kShuffle, 1);
    a.setParam(kShuffle, 0);
    a.setParam(kSwing, 0.4);

    Engine b(kRate, 999);
    for (int i = 0; i < kParamCount; ++i) b.setParam(i, a.param(i));
    for (int s = 0; s < kSeedCount; ++s) b.setSeed(s, a.seed(s));

    CHECK_EQ(a.slotCount(), b.slotCount());
    for (int i = 0; i < a.slotCount(); ++i) {
        CHECK_EQ(a.slotStart(i), b.slotStart(i));
        CHECK_EQ(a.soundEnd(i), b.soundEnd(i));
        CHECK_EQ(a.noteValue(i), b.noteValue(i));
    }
}

void regenerationFollowsDependencies() {
    Engine e(kRate, 5);
    std::uint32_t before[kSeedCount];
    for (int s = 0; s < kSeedCount; ++s) before[s] = e.seed(s);

    // Swing and retrig: no sequence changes (swing only rebuilds the table).
    e.setParam(kSwing, 0.7);
    e.setParam(kRetrig, 1);
    for (int s = 0; s < kSeedCount; ++s) CHECK_EQ(e.seed(s), before[s]);

    // Legato, sustain: the sustain sequence only.
    e.setParam(kSustainMax, 1);
    CHECK_EQ(e.seed(kSeedTimes), before[kSeedTimes]);
    CHECK_EQ(e.seed(kSeedSwing), before[kSeedSwing]);
    CHECK_EQ(e.seed(kSeedNotes), before[kSeedNotes]);
    CHECK(e.seed(kSeedSustain) != before[kSeedSustain]);
    const std::uint32_t sustain = e.seed(kSeedSustain);
    e.setParam(kLegato, 1);
    CHECK(e.seed(kSeedSustain) != sustain);
    CHECK_EQ(e.seed(kSeedTimes), before[kSeedTimes]);

    // The same value again changes nothing.
    const std::uint32_t again = e.seed(kSeedSustain);
    e.setParam(kLegato, 1);
    CHECK_EQ(e.seed(kSeedSustain), again);

    // Loop: everything.
    e.setParam(kLoop, 4);
    for (int s = 0; s < kSeedCount; ++s) CHECK(e.seed(s) != before[s]);

    // Shuffle: everything, once per press.
    std::uint32_t mid[kSeedCount];
    for (int s = 0; s < kSeedCount; ++s) mid[s] = e.seed(s);
    e.setParam(kShuffle, 1);
    for (int s = 0; s < kSeedCount; ++s) CHECK(e.seed(s) != mid[s]);
    const std::uint32_t held = e.seed(kSeedTimes);
    e.setParam(kShuffle, 1);
    CHECK_EQ(e.seed(kSeedTimes), held);
    CHECK_EQ(e.param(kShuffle), 0.0);
}

void chaosButtonRollsWithinRanges() {
    Engine e(kRate, 15);
    e.setParam(kChaosShuffle, 0);
    e.setParam(kChaosLength, 1);
    e.setParam(kChaosSwing, 1);
    e.setParam(kChaosSustain, 1);
    e.setParam(kChaosChaos, 1);
    e.setParam(kChaosValue, 1);
    CHECK_EQ(e.takeChaosChanges(), 0u);
    for (int i = 0; i < 60; ++i) {
        e.setParam(kChaos, 1);
        e.setParam(kChaos, 0);
        // Chaos 1 ranges: lengths 1/16..1, sustain 1/32..16, swing 0..1, chaos loop 1/8..1, chaos beat 1/8..2.
        CHECK(e.param(kNoteLengthMin) >= -4 && e.param(kNoteLengthMin) <= 0);
        CHECK(e.param(kNoteLengthMax) >= -4 && e.param(kNoteLengthMax) <= 0);
        CHECK(e.param(kNoteLengthMin) <= e.param(kNoteLengthMax));
        CHECK(e.param(kSustainMin) >= -5 && e.param(kSustainMax) <= 4);
        CHECK(e.param(kSustainMin) <= e.param(kSustainMax));
        CHECK(e.param(kSwing) >= 0 && e.param(kSwing) <= 1);
        CHECK(e.param(kChaosLoop) >= -3 && e.param(kChaosLoop) <= 0);
        CHECK(e.param(kChaosBeat) >= -3 && e.param(kChaosBeat) <= 1);
        const std::uint32_t changes = e.takeChaosChanges();
        CHECK(changes & (1u << kNoteLengthMin));
        CHECK(changes & (1u << kSwing));
        CHECK(changes & (1u << kChaosBeat));
        CHECK(!(changes & (1u << kLoop)));
    }
    // Chaos 0 ranges: swing 0..0.2, lengths 1/4..8.
    e.setParam(kChaosValue, 0);
    for (int i = 0; i < 30; ++i) {
        e.setParam(kChaos, 1);
        e.setParam(kChaos, 0);
        CHECK(e.param(kSwing) <= 0.2);
        CHECK(e.param(kNoteLengthMin) >= -2 && e.param(kNoteLengthMax) <= 3);
    }
}

void chaosTriggerChoiceDoesNotShuffle() {
    Engine e(kRate, 22);
    std::uint32_t before[kSeedCount];
    for (int s = 0; s < kSeedCount; ++s) before[s] = e.seed(s);
    for (int t : {2, 3, 1, 0, 3, 2}) e.setParam(kChaosTrigger, t);
    for (int s = 0; s < kSeedCount; ++s) CHECK_EQ(e.seed(s), before[s]);
    CHECK_EQ(e.takeChaosChanges(), 0u);
}

void chaosOffGroupsStayPut() {
    Engine e(kRate, 16);
    e.setParam(kChaosShuffle, 0);
    std::uint32_t before[kSeedCount];
    for (int s = 0; s < kSeedCount; ++s) before[s] = e.seed(s);
    e.setParam(kChaos, 1);
    for (int s = 0; s < kSeedCount; ++s) CHECK_EQ(e.seed(s), before[s]);
    CHECK_EQ(e.takeChaosChanges(), 0u);
    e.setParam(kChaos, 0);
    e.setParam(kChaosShuffle, 1);
    e.setParam(kChaos, 1);
    for (int s = 0; s < kSeedCount; ++s) CHECK(e.seed(s) != before[s]);
}

// Counts Times-seed changes (chaos shuffle) block by block.
int countRolls(Engine& e, long long totalFrames, int block, const std::vector<In>& inputs) {
    int rolls = 0;
    std::uint32_t last = e.seed(kSeedTimes);
    OutBuffer out;
    Transport play;
    play.playing = true;
    play.bpm = kBpm;
    e.transport(play, 0);
    for (long long start = 0; start < totalFrames; start += block) {
        const int frames = static_cast<int>(std::min<long long>(block, totalFrames - start));
        for (const In& in : inputs) {
            if (in.frame >= start && in.frame < start + frames) {
                e.midi(static_cast<int>(in.frame - start), static_cast<std::uint8_t>(in.status),
                       static_cast<std::uint8_t>(in.d1), static_cast<std::uint8_t>(in.d2));
            }
        }
        out.clear();
        e.process(frames, nullptr, nullptr, out);
        if (e.seed(kSeedTimes) != last) {
            ++rolls;
            last = e.seed(kSeedTimes);
        }
    }
    return rolls;
}

void chaosBeatTrigger() {
    // Every beat from the transport start; the start itself is not a roll. Beats 1..4 in 4.25 beats.
    Engine e(kRate, 17);
    e.setParam(kChaosTrigger, static_cast<int>(ChaosTrigger::Beat));
    e.setParam(kChaosBeat, 0);
    CHECK_EQ(countRolls(e, frameOf(4.25), static_cast<int>(frameOf(0.25)), chord(0, {60})), 4);
}

void chaosCountsFromTheSequenceStart() {
    // Chords from silence every 4 beats, chaos every 4 beats. Without retrig the count follows the
    // transport, so it rolls at beats 4 and 8. With retrig each chord restarts it, so it never rolls.
    for (int retrig = 0; retrig < 2; ++retrig) {
        Engine e(kRate, 20);
        e.setParam(kRetrig, retrig);
        e.setParam(kChaosTrigger, static_cast<int>(ChaosTrigger::Beat));
        e.setParam(kChaosBeat, 2);
        std::vector<In> in;
        for (int bar = 0; bar < 3; ++bar) {
            in.push_back({frameOf(bar * 4.0), 0x90, 60, 100});
            in.push_back({frameOf(bar * 4.0 + 3.9), 0x80, 60, 0});
        }
        CHECK_EQ(countRolls(e, frameOf(9), static_cast<int>(frameOf(0.25)), in), retrig ? 0 : 2);
    }
}

void chaosLoopTrigger() {
    // Loop 2 beats, chaos loop 2 loops → every 4 beats: beats 4 and 8 in 9 beats.
    Engine e(kRate, 18);
    e.setParam(kLoop, 1);
    e.setParam(kChaosTrigger, static_cast<int>(ChaosTrigger::Loop));
    e.setParam(kChaosLoop, 1);
    CHECK_EQ(countRolls(e, frameOf(9), static_cast<int>(frameOf(0.25)), chord(0, {60})), 2);
}

void chaosNoteTrigger() {
    // Each new chord from silence rolls; notes added to a held chord do not.
    Engine e(kRate, 19);
    e.setParam(kChaosTrigger, static_cast<int>(ChaosTrigger::Note));
    std::vector<In> in;
    in.push_back({frameOf(0.1), 0x90, 60, 100});
    in.push_back({frameOf(0.6), 0x90, 64, 100});    // added: no roll
    in.push_back({frameOf(1.1), 0x80, 60, 0});
    in.push_back({frameOf(1.1), 0x80, 64, 0});
    in.push_back({frameOf(1.6), 0x90, 62, 100});    // from silence: roll
    CHECK_EQ(countRolls(e, frameOf(2), static_cast<int>(frameOf(0.25)), in), 2);
}

} // namespace

void engineTests() {
    sameOutputForEveryBlockSize();
    onlyHeldNotesWithTheirVelocity();
    noInputNoOutput();
    notesFollowTheSequence();
    sameNoteRestartsEverySlot();
    sustainSetsNoteLength();
    stopReleasesEverything();
    releasingTheChordStopsTheNote();
    allNotesOffCc();
    chordChangeWaitsForTheNextSlot();
    newChordFromSilenceStartsAtOnce();
    retrigStartsTheSequenceAtTheNote();
    seedsReplay();
    regenerationFollowsDependencies();
    chaosButtonRollsWithinRanges();
    chaosOffGroupsStayPut();
    chaosTriggerChoiceDoesNotShuffle();
    chaosBeatTrigger();
    chaosCountsFromTheSequenceStart();
    chaosLoopTrigger();
    chaosNoteTrigger();
}
