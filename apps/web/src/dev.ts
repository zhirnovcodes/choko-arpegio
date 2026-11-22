// The standalone page: a host stand-in with Play / Stop / BPM, random chords as input,
// the arpeggiator plugin, and IvanDAW's base sine synth playing what it sends.

import {
  type Anchor,
  beatAt,
  beatsIn,
  createTicker,
  type HostContext,
  LOOKAHEAD,
  lookaheadWindow,
  type PluginEvent,
  type PluginInstance,
  TICK_MS,
  type TransportState,
} from 'ivandaw-core';
import { INDEX, PARAMS, toPlain } from '../../../hosts/web/params';
import factory, { setWasmUrl } from '../../../hosts/web/plugin';
import { randomChord } from './chords';
import synthFactory from './synth/index';
import { formatValue, noteName } from './ui/format';

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
const startBtn = $<HTMLButtonElement>('start');
const playBtn = $<HTMLButtonElement>('play');
const stopBtn = $<HTMLButtonElement>('stop');
const saveBtn = $<HTMLButtonElement>('save');
const bpmInput = $<HTMLInputElement>('bpm');
const volume = $<HTMLInputElement>('volume');
const clickBox = $<HTMLInputElement>('click');
const demoBox = $<HTMLInputElement>('demo');
const everySelect = $<HTMLSelectElement>('every');
const changeSelect = $<HTMLSelectElement>('change');
const logEl = $<HTMLDivElement>('log');
const stats = $<HTMLSpanElement>('stats');

// Next to index.html, in dev and in the built site.
setWasmUrl(new URL('choko-arpegio.wasm', document.baseURI));

function log(text: string, cls = 'sys') {
  const line = document.createElement('div');
  line.className = cls;
  line.textContent = text;
  logEl.append(line);
  while (logEl.childElementCount > 400) logEl.firstElementChild?.remove();
  logEl.scrollTop = logEl.scrollHeight;
}

let ctx: AudioContext;
let arp: PluginInstance;
let synth: PluginInstance;
let anchor: Anchor;

const bpm = () => Math.min(300, Math.max(20, Number(bpmInput.value) || 120));
const stateAt = (time: number): TransportState => ({ ...anchor.state, beat: beatAt(time, anchor) });
const timeOfBeat = (beat: number) => anchor.time + ((beat - anchor.state.beat) * 60) / anchor.state.bpm;

function send(state: TransportState, time: number) {
  anchor = { state, time };
  arp.schedule([{ type: 'transport', time, state }]);
  synth.schedule([{ type: 'transport', time, state }]);
}

const midi = (time: number, bytes: [number, number, number]): PluginEvent => ({ type: 'midi', time, bytes });

// ---- input: random chords on the beat grid, and the computer keyboard ----

let chord: number[] = [];
let lastChordBeat = -1;
/** The computer keyboard took over from the random chords (until Random chords is ticked again or Play). */
let ownNotes = false;

function releaseChord(time: number) {
  if (!chord.length) return;
  arp.schedule(chord.map((p) => midi(time, [0x80, p, 0])));
  chord = [];
}

// Random chords on: a new chord every `every` beats. Off: the current chord is held; with none yet,
// one is booked on the next beat and then held. Keys held down pause both.
function chordTick() {
  if (!arp || !anchor.state.playing || keysDown.size > 0) return;
  const hold = !demoBox.checked;
  if (hold && (chord.length > 0 || ownNotes)) return;
  const every = hold ? 1 : Number(everySelect.value);
  // Booked two lookaheads ahead: the arpeggiator itself renders one LOOKAHEAD ahead, and its input
  // must arrive before that, as it does from an upstream track in IvanDAW.
  for (const n of beatsIn(lookaheadWindow(ctx.currentTime, 2 * LOOKAHEAD), anchor)) {
    if (n <= lastChordBeat || n % every !== 0) continue;
    lastChordBeat = n;
    const time = timeOfBeat(n);
    const next = randomChord(Math.random, chord);
    const ons = next.map((p) => midi(time, [0x90, p, 80 + Math.floor(Math.random() * 40)]));
    if (changeSelect.value === 'legato') {
      // New notes first, then the old ones go: the chord never empties.
      const offs = chord.filter((p) => !next.includes(p)).map((p) => midi(time, [0x80, p, 0]));
      arp.schedule([...ons, ...offs]);
    } else {
      // Old chord ends a 1/16 beat early: each chord starts from silence.
      const gap = Math.max(ctx.currentTime, time - 60 / 16 / anchor.state.bpm);
      releaseChord(gap);
      arp.schedule(ons);
    }
    chord = next;
    log(`${time.toFixed(3)}  chord ${next.map(noteName).join(' ')}${hold ? ' (held)' : ''}`, 'in');
    if (hold) break;
  }
}

const KEYS = 'awsedftgyhujk';
const keysDown = new Set<string>();
function keyNote(e: KeyboardEvent, on: boolean) {
  const k = e.key.toLowerCase();
  const i = KEYS.indexOf(k);
  if (i < 0 || !arp || e.repeat || (e.target as HTMLElement)?.tagName === 'INPUT') return;
  if (on === keysDown.has(k)) return;
  if (on) {
    keysDown.add(k);
    // Your own notes replace the random chord.
    releaseChord(ctx.currentTime);
    ownNotes = true;
  } else {
    keysDown.delete(k);
  }
  const pitch = 60 + i;
  arp.schedule([midi(ctx.currentTime, on ? [0x90, pitch, 100] : [0x80, pitch, 0])]);
  if (on) log(`${ctx.currentTime.toFixed(3)}  key ${noteName(pitch)}`, 'in');
}

// ---- transport ----

function play() {
  if (anchor.state.playing) return;
  const time = ctx.currentTime + 0.05;
  lastClick = -1;
  lastChordBeat = -1;
  ownNotes = false;
  send({ playing: true, bpm: bpm(), beat: 0, numerator: 4, denominator: 4, loop: null }, time);
  chordTick(); // the first chord goes with the transport, before the arpeggiator renders past it
  playBtn.disabled = true;
  stopBtn.disabled = false;
  log(`▶ play at ${time.toFixed(3)} · ${bpm()} BPM`);
}

function stop() {
  if (!anchor.state.playing) return;
  const time = ctx.currentTime;
  releaseChord(time);
  send({ ...anchor.state, playing: false, beat: 0 }, time);
  // As IvanDAW does: transport stop, then CC 123 (it also forgets keys still held).
  arp.schedule([midi(time, [0xb0, 123, 0])]);
  keysDown.clear();
  playBtn.disabled = false;
  stopBtn.disabled = true;
  log(`■ stop at ${time.toFixed(3)}`);
}

// Metronome click, booked ahead like every musical event (never setTimeout).
let lastClick = -1;
function clickTick() {
  if (!clickBox.checked || !anchor.state.playing) return;
  for (const n of beatsIn(lookaheadWindow(ctx.currentTime), anchor)) {
    if (n <= lastClick) continue;
    lastClick = n;
    const t = timeOfBeat(n);
    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.frequency.value = n % 4 === 0 ? 1760 : 1320;
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(0.12, t + 0.002);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.05);
    osc.connect(g).connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.06);
  }
}

async function start() {
  startBtn.disabled = true;
  ctx = new AudioContext();
  await ctx.resume();
  const master = ctx.createGain();
  master.gain.value = Number(volume.value);
  master.connect(ctx.destination);
  volume.oninput = () => master.gain.setTargetAtTime(Number(volume.value), ctx.currentTime, 0.02);
  anchor = { state: { playing: false, bpm: bpm(), beat: 0, numerator: 4, denominator: 4, loop: null }, time: ctx.currentTime };

  const host = (slotId: string): HostContext => ({
    audioContext: ctx,
    apiVersion: 2,
    slotId,
    transport: () => stateAt(ctx.currentTime),
    notify: (level, message) => log(`[${level}] ${message}`),
    markDirty: () => {},
    assetUrl: (path) => new URL(path, document.baseURI).href,
  });

  try {
    synth = await synthFactory.create(host('synth'), null);
    const out = synth.ports.get('out') as { node?: AudioNode } | undefined;
    out?.node?.connect(master);
    arp = await factory.create(host('arp'), $('plugin'));
  } catch (error) {
    log(`could not create the plugins: ${error}`);
    startBtn.disabled = false;
    return;
  }
  arp.listen((e: PluginEvent) => {
    synth.schedule([e]);
    if (e.type === 'midi') {
      const [s, p, v] = e.bytes;
      const on = (s & 0xf0) === 0x90 && v > 0;
      if (on) log(`${e.time.toFixed(3)}  ♪ ${noteName(p).padEnd(4)} (${p}) v${v}`, 'on');
    }
  });
  arp.subscribe((c) => {
    if (c.source !== 'internal') return;
    const p = PARAMS[INDEX.get(c.id)!];
    log(`chaos: ${p.label} = ${formatValue(p, toPlain(p, c.value))}`);
  });

  createTicker(TICK_MS, () => {
    clickTick();
    chordTick();
  });
  const showStats = () => {
    const beat = anchor.state.playing ? beatAt(ctx.currentTime, anchor) : null;
    stats.textContent = beat === null ? 'stopped' : `bar ${Math.floor(beat / 4) + 1} · beat ${((beat % 4) + 1).toFixed(2)}`;
    requestAnimationFrame(showStats);
  };
  showStats();

  playBtn.disabled = false;
  saveBtn.disabled = false;
  startBtn.textContent = `Audio on · ${ctx.sampleRate} Hz`;
  log(`ready · ${factory.manifest.name} ${factory.manifest.version} → ${synthFactory.manifest.name}`);
}

// ?auto starts audio and plays at once (for headless smoke checks; browsers may block audio without a click).
if (new URLSearchParams(location.search).has('auto')) {
  start().then(() => arp && play());
}

startBtn.onclick = start;
playBtn.onclick = play;
stopBtn.onclick = stop;
demoBox.onchange = () => {
  // Off keeps the current chord playing; on brings the changes back at the next chord beat.
  if (demoBox.checked) ownNotes = false;
  if (arp) log(demoBox.checked ? 'random chords on' : `holding ${chord.length ? chord.map(noteName).join(' ') : 'the next chord'}`);
};
bpmInput.onchange = () => {
  if (!arp || !anchor.state.playing) return;
  const time = ctx.currentTime + 0.05;
  send({ ...stateAt(time), bpm: bpm() }, time);
  log(`tempo ${bpm()} BPM at ${time.toFixed(3)}`);
};
saveBtn.onclick = async () => {
  const state = await arp.getState();
  log(`getState: ${JSON.stringify(state.data)}`);
  await arp.setState(JSON.parse(JSON.stringify(state)));
  chord = [];
  log('setState done: same sequence; the chord comes back with the next change');
};
window.addEventListener('keydown', (e) => {
  if ((e.target as HTMLElement)?.tagName === 'INPUT' || (e.target as HTMLElement)?.tagName === 'SELECT') return;
  if (e.code === 'Space' && arp) {
    e.preventDefault();
    if (anchor.state.playing) stop();
    else play();
    return;
  }
  keyNote(e, true);
});
window.addEventListener('keyup', (e) => keyNote(e, false));
