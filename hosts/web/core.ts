// The wasm module's exports (hosts/web/boundary/api.cpp) and how to load it.

export interface Core {
  ca_create(rate: number, seed: number): number;
  ca_destroy(engine: number): void;
  ca_set_param(engine: number, index: number, value: number): void;
  ca_param(engine: number, index: number): number;
  ca_transport(engine: number, playing: number, bpm: number, beat: number, num: number, den: number, frame: number): void;
  ca_midi(engine: number, frame: number, status: number, d1: number, d2: number): void;
  ca_all_off(engine: number, frame: number): void;
  ca_process(engine: number, frames: number, left: number, right: number): void;
  ca_out_count(engine: number): number;
  ca_out_frame(engine: number, k: number): number;
  ca_out_kind(engine: number, k: number): number;
  ca_out_bytes(engine: number, k: number): number;
  ca_chaos_changes(engine: number): number;
  ca_seed(engine: number, which: number): number;
  ca_set_seed(engine: number, which: number, value: number): void;
  ca_slot_count(engine: number): number;
  ca_slot_start(engine: number, slot: number): number;
  ca_slot_end(engine: number, slot: number): number;
  ca_sound_end(engine: number, slot: number): number;
  ca_note_value(engine: number, slot: number): number;
  ca_time_origin(engine: number): number;
}

/** Sequence seeds saved in state, in Engine's SeqSeed order. */
export const SEED_COUNT = 4;

// STANDALONE_WASM imports only this one function (checked with WebAssembly.Module.imports).
const imports = { env: { emscripten_notify_memory_growth() {} } };

export async function instantiateCore(bytes: BufferSource): Promise<Core> {
  const { instance } = await WebAssembly.instantiate(bytes, imports);
  (instance.exports as { _initialize?: () => void })._initialize?.();
  return instance.exports as unknown as Core;
}

const loaded = new Map<string, Promise<Core>>();

/** One wasm instance per URL, shared by every plugin instance (each gets its own engine). */
export function loadCore(url: URL): Promise<Core> {
  const key = url.href;
  let core = loaded.get(key);
  if (!core) {
    core = fetch(url)
      .then((r) => {
        if (!r.ok) throw new Error(`choko-arpegio: could not load ${url.href} (${r.status})`);
        return r.arrayBuffer();
      })
      .then(instantiateCore);
    core.catch(() => loaded.delete(key));
    loaded.set(key, core);
  }
  return core;
}
