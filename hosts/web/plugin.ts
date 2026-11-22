// Choco Arpegio for IvanDAW: the Web Plugin API v2 entry (default export: PluginFactory).
// MIDI generator: the engine runs on the main thread, rendered LOOKAHEAD ahead by a ticker.

import {
  createTicker,
  LOOKAHEAD,
  type ParameterChange,
  type PluginEvent,
  type PluginFactory,
  type PluginInstance,
  type PluginState,
  type PortHandle,
  TICK_MS,
} from 'ivandaw-core';
import { ID, manifest, VERSION } from '../../apps/web/src/manifest';
import { mountSkin, type SequenceView, type SkinBridge } from '../../apps/web/src/ui/skin';
import { type Core, loadCore, SEED_COUNT } from './core';
import { createDriver } from './driver';
import { NoteTimeline } from './noteTimeline';
import { INDEX, PARAMS, toNormalized, toPlain } from './params';

/** Where the wasm is. The DAW build has it next to index.js; the dev page passes its own URL. */
let wasmUrl = new URL(/* @vite-ignore */ './choko-arpegio.wasm', import.meta.url);
export function setWasmUrl(url: URL) {
  wasmUrl = url;
}

interface SavedState {
  v: 1;
  params: Record<string, number>; // plain values by id
  seeds: number[]; // uint32, Engine SeqSeed order
}

function view(core: Core, engine: number): SequenceView {
  const slots: SequenceView['slots'] = [];
  const count = core.ca_slot_count(engine);
  for (let i = 0; i < count; i++) {
    slots.push({
      start: core.ca_slot_start(engine, i),
      end: core.ca_slot_end(engine, i),
      soundEnd: core.ca_sound_end(engine, i),
      value: core.ca_note_value(engine, i),
    });
  }
  const loop = count > 0 ? slots[count - 1].end : 1;
  return { loop, origin: core.ca_time_origin(engine), slots };
}

const isMidi = (e: PluginEvent): e is Extract<PluginEvent, { type: 'midi' }> => e.type === 'midi';

const factory: PluginFactory = {
  manifest,
  async create(host, container) {
    const ctx = host.audioContext;
    const core = await loadCore(wasmUrl);
    const engine = core.ca_create(ctx.sampleRate, (Math.random() * 0x100000000) >>> 0);
    if (!engine) throw new Error('choko-arpegio: engine could not be created');

    const outListeners = new Set<(e: PluginEvent) => void>();
    const paramListeners = new Set<(c: ParameterChange) => void>();
    const viewListeners = new Set<() => void>();
    const heldNotes = new NoteTimeline();
    const playedNotes = new NoteTimeline();

    const plain = (id: string) => core.ca_param(engine, INDEX.get(id)!);
    const normalized = (id: string) => {
      const p = PARAMS[INDEX.get(id)!];
      return p.kind === 'trigger' ? 0 : toNormalized(p, plain(id));
    };
    const viewChanged = () => {
      for (const l of [...viewListeners]) l();
    };
    function notifyParam(c: ParameterChange) {
      for (const l of [...paramListeners]) l(c);
    }
    // Chaos moved these params inside the engine: tell the host and the skin.
    const notifyMask = (mask: number) => {
      PARAMS.forEach((p, i) => {
        if (mask & (1 << i)) notifyParam({ id: p.id, value: normalized(p.id), source: 'internal' });
      });
      viewChanged();
    };

    const driver = createDriver({
      core,
      engine,
      sampleRate: ctx.sampleRate,
      now: () => ctx.currentTime,
      createTicker,
      tickMs: TICK_MS,
      lookahead: LOOKAHEAD,
      emit(e) {
        if (isMidi(e)) {
          const [s, pitch, v] = e.bytes;
          playedNotes.add(e.time, pitch, (s & 0xf0) === 0x90 && v > 0);
        }
        for (const l of [...outListeners]) l(e);
      },
      onChaos: (mask) => notifyMask(mask),
    });

    // Late joiners: start from the host's transport.
    const t = host.transport();
    if (t.playing) driver.schedule([{ type: 'transport', time: ctx.currentTime, state: t }]);

    /** Sets a parameter; returns false for an unknown id. */
    const setNormalized = (id: string, n: number): boolean => {
      const index = INDEX.get(id);
      if (index === undefined || !Number.isFinite(n)) return false;
      const p = PARAMS[index];
      const before = PARAMS.map((q) => plain(q.id));
      if (p.kind === 'trigger') {
        // Fires on the rise past 0.5; reads back 0.
        core.ca_set_param(engine, index, n > 0.5 ? 1 : 0);
        if (n > 0.5) core.ca_set_param(engine, index, 0);
      } else {
        core.ca_set_param(engine, index, toPlain(p, n));
      }
      core.ca_chaos_changes(engine); // reported below with everything else that moved
      // The Chaos button moves other parameters. Tell everyone.
      PARAMS.forEach((q, i) => {
        if (i !== index && plain(q.id) !== before[i]) {
          notifyParam({ id: q.id, value: normalized(q.id), source: 'internal' });
        }
      });
      viewChanged();
      return true;
    };

    const bridge: SkinBridge = {
      params: PARAMS,
      get: normalized,
      set(id, n, gesture) {
        if (!setNormalized(id, n)) return;
        host.markDirty();
        notifyParam({ id, value: normalized(id), gesture, source: 'ui' });
      },
      onParam(cb) {
        const l = (c: ParameterChange) => {
          if (c.source !== 'ui') cb(c.id, c.value);
        };
        paramListeners.add(l);
        return () => paramListeners.delete(l);
      },
      view: () => view(core, engine),
      onView(cb) {
        viewListeners.add(cb);
        return () => viewListeners.delete(cb);
      },
      playhead: () => driver.beatAt(ctx.currentTime),
      held: () => heldNotes.at(ctx.currentTime),
      played: () => playedNotes.at(ctx.currentTime),
    };

    let unmountSkin: (() => void) | null = null;
    const mount = (el: HTMLElement) => {
      unmountSkin?.();
      unmountSkin = mountSkin(el, bridge);
    };
    if (container) mount(container);

    const ports = new Map<string, PortHandle>(
      manifest.ports.map((info): [string, PortHandle] => [info.id, { info: info as PortHandle['info'] & { kind: 'events' } }]),
    );

    const forgetHeld = (time: number) => {
      heldNotes.clear(Math.max(time, ctx.currentTime));
      playedNotes.clear(Math.max(time, ctx.currentTime));
    };

    let disposed = false;
    const instance: PluginInstance = {
      ports,
      latency: 0,
      tail: 0,
      parameters: () => PARAMS.map((p) => ({ ...p })),
      parameter: (id) => (INDEX.has(id) ? normalized(id) : 0),
      schedule(events) {
        if (disposed) return;
        const rest: PluginEvent[] = [];
        for (const e of events) {
          if (e.type === 'automation') {
            if (setNormalized(e.id, e.value)) {
              notifyParam({ id: e.id, value: normalized(e.id), source: e.source === 'binding' ? 'binding' : 'host' });
            }
            continue;
          }
          if (isMidi(e)) {
            const [s, pitch, v = 0] = e.bytes;
            const type = s & 0xf0;
            if (type === 0x90 || type === 0x80) heldNotes.add(e.time, pitch, type === 0x90 && v > 0);
            else if (type === 0xb0 && (pitch === 120 || pitch === 123)) forgetHeld(e.time);
          }
          if (e.type === 'transport' && !e.state.playing) playedNotes.clear(Math.max(e.time, ctx.currentTime));
          rest.push(e);
        }
        if (rest.length) driver.schedule(rest);
      },
      listen(l) {
        outListeners.add(l);
        return () => outListeners.delete(l);
      },
      subscribe(l) {
        paramListeners.add(l);
        return () => paramListeners.delete(l);
      },
      async getState(): Promise<PluginState> {
        const data: SavedState = {
          v: 1,
          params: Object.fromEntries(PARAMS.filter((p) => p.kind !== 'trigger').map((p) => [p.id, plain(p.id)])),
          seeds: Array.from({ length: SEED_COUNT }, (_, i) => core.ca_seed(engine, i) >>> 0),
        };
        return { pluginId: ID, pluginVersion: VERSION, data };
      },
      async setState(s) {
        driver.allOff(ctx.currentTime);
        forgetHeld(ctx.currentTime);
        const data = (s?.data ?? {}) as Partial<SavedState>;
        const saved = data.params ?? {};
        PARAMS.forEach((p, index) => {
          if (p.kind === 'trigger') return;
          const v = typeof saved[p.id] === 'number' ? saved[p.id] : p.default;
          core.ca_set_param(engine, index, v);
        });
        // Seeds last: setting params may reseed.
        if (Array.isArray(data.seeds)) {
          data.seeds.slice(0, SEED_COUNT).forEach((seed, i) => {
            if (typeof seed === 'number') core.ca_set_seed(engine, i, seed | 0);
          });
        }
        core.ca_chaos_changes(engine);
        for (const p of PARAMS) notifyParam({ id: p.id, value: normalized(p.id), source: 'host' });
        viewChanged();
      },
      mount,
      unmount() {
        unmountSkin?.();
        unmountSkin = null;
      },
      suspend: () => driver.suspend(),
      resume: () => driver.resume(),
      dispose() {
        if (disposed) return;
        disposed = true;
        unmountSkin?.();
        unmountSkin = null;
        driver.dispose(); // emits the last note-offs to the listeners
        core.ca_destroy(engine);
        outListeners.clear();
        paramListeners.clear();
        viewListeners.clear();
      },
    };
    return instance;
  },
};

export default factory;
