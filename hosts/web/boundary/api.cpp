// C ABI over ca::Engine → choko-arpegio.wasm. Standalone wasm, no Emscripten JS glue.
#include <cstdlib>
#include <new>

#include "Engine.h"

#define CA_EXPORT extern "C" __attribute__((used, visibility("default")))

struct Instance {
    ca::Engine engine;
    ca::OutBuffer out;
    Instance(double r, std::uint32_t s) : engine(r, s) {}
};

CA_EXPORT void* ca_alloc(int bytes) { return bytes > 0 ? std::malloc(bytes) : nullptr; }
CA_EXPORT void ca_free(void* p) { std::free(p); }

CA_EXPORT Instance* ca_create(double rate, std::uint32_t seed) {
    if (!(rate > 0)) return nullptr;
    void* m = std::malloc(sizeof(Instance));
    return m ? new (m) Instance(rate, seed) : nullptr;
}
CA_EXPORT void ca_destroy(Instance* i) {
    if (i) {
        i->~Instance();
        std::free(i);
    }
}

CA_EXPORT void ca_set_param(Instance* i, int index, double value) { if (i) i->engine.setParam(index, value); }
CA_EXPORT double ca_param(Instance* i, int index) { return i ? i->engine.param(index) : 0; }

CA_EXPORT void ca_transport(Instance* i, int playing, double bpm, double beat, int num, int den, int frame) {
    if (!i) return;
    ca::Transport t;
    t.playing = playing != 0;
    t.bpm = bpm;
    t.beat = beat;
    t.numerator = num;
    t.denominator = den;
    i->engine.transport(t, frame);
}
CA_EXPORT void ca_midi(Instance* i, int frame, int status, int d1, int d2) {
    if (i) i->engine.midi(frame, static_cast<std::uint8_t>(status), static_cast<std::uint8_t>(d1), static_cast<std::uint8_t>(d2));
}
// Replaces the output buffer with the note-offs.
CA_EXPORT void ca_all_off(Instance* i, int frame) {
    if (!i) return;
    i->out.clear();
    i->engine.allOff(frame, i->out);
}
// Replaces the output buffer with this call's events.
CA_EXPORT void ca_process(Instance* i, int frames, float* left, float* right) {
    if (!i) return;
    i->out.clear();
    i->engine.process(frames, left, right, i->out);
}

CA_EXPORT int ca_out_count(Instance* i) { return i ? i->out.size() : 0; }
CA_EXPORT int ca_out_frame(Instance* i, int k) { return i ? i->out[k].frame : 0; }
CA_EXPORT int ca_out_kind(Instance* i, int k) { return i ? static_cast<int>(i->out[k].kind) : 0; }
CA_EXPORT int ca_out_bytes(Instance* i, int k) {
    if (!i) return 0;
    const ca::OutEvent& e = i->out[k];
    return e.b0 | (e.b1 << 8) | (e.b2 << 16);
}

// Params chaos changed since the last call, one bit per param index.
CA_EXPORT unsigned ca_chaos_changes(Instance* i) { return i ? i->engine.takeChaosChanges() : 0; }

// State.
CA_EXPORT unsigned ca_seed(Instance* i, int which) { return i ? i->engine.seed(which) : 0; }
CA_EXPORT void ca_set_seed(Instance* i, int which, unsigned value) { if (i) i->engine.setSeed(which, value); }

// UI view.
CA_EXPORT int ca_slot_count(Instance* i) { return i ? i->engine.slotCount() : 0; }
CA_EXPORT double ca_slot_start(Instance* i, int slot) { return i ? i->engine.slotStart(slot) : 0; }
CA_EXPORT double ca_slot_end(Instance* i, int slot) { return i ? i->engine.slotEnd(slot) : 0; }
CA_EXPORT double ca_sound_end(Instance* i, int slot) { return i ? i->engine.soundEnd(slot) : 0; }
CA_EXPORT double ca_note_value(Instance* i, int slot) { return i ? i->engine.noteValue(slot) : 0; }
CA_EXPORT double ca_time_origin(Instance* i) { return i ? i->engine.timeOrigin() : 0; }
