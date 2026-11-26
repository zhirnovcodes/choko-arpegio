import fs from 'node:fs';
import type { PluginEvent, TransportState } from 'ivandaw-core';
import { beforeAll, describe, expect, it } from 'vitest';
import { type Core, instantiateCore } from '../../../hosts/web/core';
import { createDriver } from '../../../hosts/web/driver';
import { INDEX } from '../../../hosts/web/params';
import { wasmCommand, wasmPath } from '../vite.shared.ts';

// The real engine (build-wasm/choko-arpegio.wasm) under a fake clock and a fake ticker.

const RATE = 48000;
let core: Core;

beforeAll(async () => {
  if (!fs.existsSync(wasmPath)) throw new Error(`build-wasm/choko-arpegio.wasm is missing. From the repo root:\n  ${wasmCommand}`);
  core = await instantiateCore(fs.readFileSync(wasmPath));
});

type Midi = Extract<PluginEvent, { type: 'midi' }>;

function rig(seed = 1) {
  let now = 0;
  let tick: (() => void) | null = null;
  const events: Midi[] = [];
  const chaos: number[] = [];
  let sequences = 0;
  const engine = core.ca_create(RATE, seed);
  const driver = createDriver({
    core,
    engine,
    sampleRate: RATE,
    now: () => now,
    createTicker(_ms, fn) {
      tick = fn;
      return { stop: () => (tick = null) };
    },
    tickMs: 25,
    lookahead: 0.1,
    emit: (e) => {
      if (e.type === 'midi') events.push(e);
    },
    onChaos: (mask) => chaos.push(mask),
    onSequence: () => sequences++,
  });
  return {
    engine,
    driver,
    events,
    chaos,
    get sequences() {
      return sequences;
    },
    set: (id: string, v: number) => core.ca_set_param(engine, INDEX.get(id)!, v),
    /** Advance the clock in 25 ms ticks. */
    run(seconds: number) {
      const end = now + seconds;
      while (now < end - 1e-9) {
        now += 0.025;
        tick?.();
      }
    },
    /** Move the clock without ticking, as the host does between its own render() calls. */
    advance(seconds: number) {
      now += seconds;
    },
    get now() {
      return now;
    },
    get ticking() {
      return tick !== null;
    },
  };
}

const state = (playing: boolean, beat: number, bpm = 120): TransportState => ({
  playing,
  bpm,
  beat,
  numerator: 4,
  denominator: 4,
  loop: null,
});

const notes = (time: number, pitches: number[], on = true): PluginEvent[] =>
  pitches.map((p) => ({ type: 'midi', time, bytes: on ? [0x90, p, 100] : [0x80, p, 0] }));

const isOn = (e: Midi) => (e.bytes[0] & 0xf0) === 0x90 && e.bytes[2] > 0;

function checkBalanced(events: Midi[]) {
  // One note at a time; every on gets an off at or after it; none open at the end.
  const sorted = [...events].sort((a, b) => a.time - b.time);
  let open = -1;
  for (const e of sorted) {
    if (isOn(e)) {
      expect(open, `pitch ${e.bytes[1]} started while ${open} sounds`).toBe(-1);
      open = e.bytes[1];
    } else {
      expect(e.bytes[1], `off without on for ${e.bytes[1]}`).toBe(open);
      open = -1;
    }
  }
  expect(open).toBe(-1);
}

describe('driver', () => {
  it('plays only the held chord, on the 1/64 beat grid, and stops cleanly', () => {
    const r = rig(3);
    r.driver.schedule([{ type: 'transport', time: 0.05, state: state(true, 0) }, ...notes(0.05, [60, 64, 67])]);
    r.run(4);
    const ons = r.events.filter(isOn);
    expect(ons.length).toBeGreaterThan(4);
    expect(ons[0].time).toBeCloseTo(0.05, 4);
    for (const e of ons) {
      expect([60, 64, 67]).toContain(e.bytes[1]);
      // 120 BPM: 1/64 beat = 1/128 s from 0.05.
      const q = (e.time - 0.05) * 128;
      expect(Math.abs(q - Math.round(q)) / 128).toBeLessThan(1 / RATE + 1e-9);
    }
    const stopAt = r.now;
    r.driver.schedule([{ type: 'transport', time: stopAt, state: state(false, 0) }]);
    r.run(1);
    checkBalanced(r.events);
    expect(r.events.filter((e) => isOn(e) && e.time > stopAt + 0.1)).toEqual([]);
  });

  it('is silent without a held chord, and stops when the chord is released', () => {
    const r = rig(4);
    r.set('legato', 1); // a note always sounds, so the release has one to stop
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }]);
    r.run(1);
    expect(r.events).toEqual([]);
    r.driver.schedule(notes(r.now + 0.05, [62, 65]));
    r.run(1);
    expect(r.events.filter(isOn).length).toBeGreaterThan(0);
    // Booked beyond the 0.1 s lookahead, as hosts do, so it lands on its exact frame.
    const releaseAt = r.now + 0.15;
    r.driver.schedule(notes(releaseAt, [62, 65], false));
    r.run(1);
    checkBalanced(r.events);
    expect(r.events[r.events.length - 1].time).toBeCloseTo(releaseAt, 4);
  });

  it('releases every note on CC 123 and on dispose', () => {
    const r = rig(5);
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }, ...notes(0, [60, 63, 67, 70])]);
    r.run(1.3);
    r.driver.schedule([{ type: 'midi', time: r.now, bytes: [0xb0, 123, 0] }]);
    r.run(0.5);
    // CC 123 also forgets the held chord: nothing new starts.
    const after = r.events.filter((e) => isOn(e) && e.time > r.now - 0.4);
    expect(after).toEqual([]);
    r.driver.dispose();
    expect(r.ticking).toBe(false);
    checkBalanced(r.events);
  });

  it('reports chaos param changes', () => {
    const r = rig(6);
    r.set('chaosTrigger', 2); // Beat
    r.set('chaosBeat', 0); // every beat
    r.set('chaosSwing', 1);
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }, ...notes(0, [60, 64])]);
    r.run(2.1); // beats 1, 2, 3, 4 at 120 BPM
    expect(r.chaos.length).toBe(4);
    for (const mask of r.chaos) expect(mask & (1 << INDEX.get('swing')!)).not.toBe(0);
    r.driver.dispose();
  });

  it('reports a chaos shuffle as a sequence change, though no param moves', () => {
    // Default chaos groups: Shuffle only.
    const r = rig(7);
    r.set('chaosTrigger', 2); // Beat
    r.set('chaosBeat', 0); // every beat
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }, ...notes(0, [60, 64])]);
    r.run(0.3);
    const before = r.sequences;
    r.run(1.8); // rendered to 2.2 s (0.1 s ahead): beats 1, 2, 3, 4 at 120 BPM
    expect(r.chaos).toEqual([]);
    expect(r.sequences - before).toBe(4);
    r.driver.dispose();
  });

  it('reports the beat for the playhead', () => {
    const r = rig();
    expect(r.driver.beatAt(0)).toBeNull();
    r.driver.schedule([{ type: 'transport', time: 1, state: state(true, 4, 60) }]);
    expect(r.driver.beatAt(3)).toBeCloseTo(6, 9);
    r.driver.dispose();
  });
});

describe('driver, host-driven render(until)', () => {
  it('stops its own ticker after the first render()', () => {
    const r = rig(8);
    expect(r.ticking).toBe(true);
    r.driver.render(0.1);
    expect(r.ticking).toBe(false);
    r.driver.schedule([{ type: 'transport', time: 0.2, state: state(true, 0) }, ...notes(0.2, [60, 64, 67])]);
    r.run(1); // the fake ticker would fire 40 times, if it were still there
    expect(r.events).toEqual([]);
    r.driver.dispose();
  });

  it('emits the beat-0 notes during the call, at t0 exactly', () => {
    const r = rig(9);
    r.driver.render(0.1);
    r.advance(0.05);
    const t0 = 0.12;
    r.driver.schedule([{ type: 'transport', time: t0, state: state(true, 0) }, ...notes(t0, [60, 64, 67])]);
    r.driver.render(t0 + 0.1);
    const ons = r.events.filter(isOn);
    expect(ons.length).toBeGreaterThan(0);
    expect(Math.abs(ons[0].time - t0)).toBeLessThanOrEqual(1 / RATE);
    r.driver.dispose();
  });

  it('a chord from an upstream plugin lands on time (the chain case)', () => {
    // Playing, no chord. Each pass: the upstream plugin books its chord inside [edge, until) of this
    // pass, then the host renders us to the same until. Our edge is behind the chord, so it is on its frame.
    const r = rig(10);
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }]);
    let edge = 0;
    for (let i = 0; i < 57; i++) {
      edge = r.now + 0.1;
      r.driver.render(edge);
      r.advance(0.025);
    }
    expect(r.events).toEqual([]);
    const t0 = 1.5; // beat 3 at 120 BPM, in the next pass's window [edge, until)
    const until = r.now + 0.1;
    expect(t0).toBeGreaterThanOrEqual(edge - 1e-9);
    expect(t0).toBeLessThan(until);
    r.driver.schedule(notes(t0, [60, 64, 67]));
    r.driver.render(until);
    const ons = r.events.filter(isOn);
    expect(ons.length).toBeGreaterThan(0);
    expect(Math.abs(ons[0].time - t0)).toBeLessThanOrEqual(1 / RATE);
    r.driver.dispose();
  });

  it('a repeated until renders nothing', () => {
    const r = rig(11);
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }, ...notes(0, [60, 64, 67])]);
    r.driver.render(0.5);
    const count = r.events.length;
    expect(count).toBeGreaterThan(0);
    r.driver.render(0.5);
    expect(r.events.length).toBe(count);
    r.driver.dispose();
  });

  it('suspend silences render(); resume renders again without a ticker', () => {
    const r = rig(12);
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }, ...notes(0, [60, 64, 67])]);
    r.driver.render(0.1);
    r.driver.suspend();
    checkBalanced(r.events);
    const count = r.events.length;
    r.advance(0.2);
    r.driver.render(0.4);
    expect(r.events.length).toBe(count);
    r.driver.resume();
    expect(r.ticking).toBe(false);
    // suspend forgot the held chord; a new one plays again.
    r.driver.schedule(notes(0.45, [62, 65]));
    r.driver.render(0.6);
    expect(r.events.filter(isOn).length).toBeGreaterThan(r.events.slice(0, count).filter(isOn).length);
    r.driver.dispose();
    checkBalanced(r.events);
  });
});
