// The Choco Arpegio skin: the device (device/device.svg) with its screen, plain DOM, used by every host through
// SkinBridge. It never knows which host runs it. All classes start with "ca-".
// Layout = the design (Frame 1.svg): a main page and a chaos page on the one screen, SHUFFLE and CHAOS below.

import { DEVICE, deviceMarkup } from './device/device';
import chaosPictureUrl from './assets/chaos-picture.png';
import css from './skin.css?inline';
import { pow2Text, type SkinParam } from './format';
import { centredLabel, div, label, PixelButton, PixelCheckbox, PixelSlider, place } from './widgets';

export { noteName } from './format';

export interface SequenceView {
  /** Loop length in beats. */
  loop: number;
  /** Beat the sequence counts from (0, or the last note start with Retrig on). */
  origin: number;
  /** In beats from the loop start, after swing. The note sounds on [start, soundEnd). value: 0..1 note pick. */
  slots: { start: number; end: number; soundEnd: number; value: number }[];
}

export interface SkinBridge {
  params: SkinParam[];
  /** normalized 0..1 */
  get(id: string): number;
  set(id: string, normalized: number, gesture?: 'begin' | 'end'): void;
  /** Changes from outside the skin (host automation, chaos, state load). */
  onParam(cb: (id: string, normalized: number) => void): () => void;
  view(): SequenceView;
  /** The sequences or anything shown in the timeline changed. */
  onView(cb: () => void): () => void;
  /** Beat now, or null when stopped. */
  playhead(): number | null;
  /** Input chord held now, ascending. */
  held(): number[];
  /** Notes the arpeggiator plays now. */
  played(): number[];
}

// One style element per document, shared by every mounted skin.
const styles = new Map<Document, { element: HTMLStyleElement; users: number }>();
function holdStyles(doc: Document): () => void {
  let entry = styles.get(doc);
  if (!entry) {
    const element = doc.createElement('style');
    element.dataset.chocoArpegio = '';
    element.textContent = css;
    doc.head.append(element);
    entry = { element, users: 0 };
    styles.set(doc, entry);
  }
  entry.users++;
  const held = entry;
  return () => {
    held.users--;
    if (held.users === 0) {
      held.element.remove();
      styles.delete(doc);
    }
  };
}

const TRIGGERS = ['NONE', 'NOTE', 'BEAT', 'LOOP'];
const TRIGGER_BEAT = 2;
const TRIGGER_LOOP = 3;

/** 2^a-2^b as text, e.g. "1/16-1/2". */
const rangeText = (a: number, b: number) => `${pow2Text(a)}-${pow2Text(b)}`;
/** The short form for tight rows: "1/16" → "/16". */
const shortPow2 = (n: number) => pow2Text(n).replace(/^1\//, '/');

/** LEN row: "LEN. (a-b)" as in the design; shorter forms when it would run into the slider (11 characters). */
export function lengthText(a: number, b: number): string {
  for (const t of [`LEN. (${rangeText(a, b)})`, `LEN. ${rangeText(a, b)}`, `LEN ${rangeText(a, b)}`, `LEN ${shortPow2(a)}-${shortPow2(b)}`]) {
    if (t.length <= 11) return t;
  }
  return `LEN ${shortPow2(a)}-${shortPow2(b)}`;
}

/** Renders the skin into `container` at design size (DEVICE, 207 x 169). Returns the unmount function. */
export function mountSkin(container: HTMLElement, bridge: SkinBridge): () => void {
  const doc = container.ownerDocument;
  const releaseStyles = holdStyles(doc);
  const cleanups: (() => void)[] = [];
  const byId = new Map(bridge.params.map((p) => [p.id, p]));

  // Plain values in and out; the bridge speaks normalized.
  const get = (id: string) => {
    const p = byId.get(id)!;
    const v = p.min + bridge.get(id) * (p.max - p.min);
    return p.kind === 'float' ? v : Math.round(v);
  };
  // Hosts do not echo the skin's own changes back (onParam is for outside changes), so the skin redraws
  // itself after every change it makes. The engine may move other params too (Chaos button): those arrive
  // through onParam as 'internal'.
  const set = (id: string, v: number, gesture?: 'begin' | 'end') => {
    const p = byId.get(id)!;
    bridge.set(id, (v - p.min) / (p.max - p.min), gesture);
    refresh();
  };
  const press = (id: string) => {
    bridge.set(id, 1, 'begin');
    bridge.set(id, 0, 'end');
    refresh();
  };
  const scaleOf = (id: string) => {
    const p = byId.get(id)!;
    return { min: p.min, max: p.max, step: p.kind === 'float' ? 0 : 1 };
  };

  const root = div(doc, 'ca-root', { x: 0, y: 0, w: DEVICE.width, h: DEVICE.height });
  root.style.position = 'relative';
  const device = div(doc, 'ca-device');
  device.innerHTML = deviceMarkup();
  root.append(device);

  // ---- main page (design − 262,−39) ----
  const main = div(doc, 'ca-page');
  const loopText = label(doc, '', 21.04, 19.71);
  const loop = new PixelSlider(doc, 102, 188, 23, scaleOf('loop'), 1, 'Loop length');
  loop.onChange = ([v], g) => set('loop', v, g);
  const lenText = label(doc, '', 21.04, 37.71);
  const len = new PixelSlider(doc, 102, 188, 41, scaleOf('noteLengthMin'), 2, 'Note length min and max');
  len.onChange = ([a, b], g) => {
    set('noteLengthMin', a, g);
    set('noteLengthMax', b, g);
  };
  const swingText = label(doc, 'SWING', 21.04, 55.71);
  const swing = new PixelSlider(doc, 102, 188, 59, scaleOf('swing'), 1, 'Swing');
  swing.onChange = ([v], g) => set('swing', v, g);
  const legato = new PixelButton(doc, { x: 20, y: 69, w: 63, h: 17 }, 'LEGATO', 'Legato');
  legato.onClick = () => set('legato', get('legato') ? 0 : 1);
  const sustain = new PixelSlider(doc, 102, 189, 77, scaleOf('sustainMin'), 2, 'Sustain min and max');
  sustain.onChange = ([a, b], g) => {
    set('sustainMin', a, g);
    set('sustainMax', b, g);
  };
  const sustainText = label(doc, '', 21.04, 95.71);
  const retrig = new PixelButton(doc, { x: 103, y: 90, w: 34, h: 17 }, 'RTRG', 'Retrig');
  retrig.onClick = () => set('retrig', get('retrig') ? 0 : 1);
  const openChaos = new PixelButton(doc, { x: 146, y: 90, w: 43, h: 17 }, 'CHAOS', 'Chaos settings');
  main.append(loopText, loop.el, lenText, len.el, swingText, swing.el, legato.el, sustain.el, sustainText, retrig.el, openChaos.el);

  // ---- chaos page (design − 470,−39) ----
  const chaos = div(doc, 'ca-page');
  chaos.hidden = true;
  const picture = doc.createElement('img');
  picture.className = 'ca-picture';
  picture.src = chaosPictureUrl;
  picture.alt = 'CHAOS';
  place(picture, { x: 20, y: 16.87, w: 57, h: 20.27 });
  const close = new PixelButton(doc, { x: 104, y: 19, w: 13, h: 13 }, 'X', 'Back to the main page');
  const valueText = label(doc, 'VALUE', 21.04, 47.71);
  const value = new PixelSlider(doc, 61, 118, 51, scaleOf('chaosValue'), 1, 'Chaos value');
  value.onChange = ([v], g) => set('chaosValue', v, g);
  const trigText = label(doc, 'TRIG.', 21.04, 70.71);
  const prev = new PixelButton(doc, { x: 56, y: 68, w: 11, h: 13 }, '<', 'Previous chaos trigger');
  const next = new PixelButton(doc, { x: 106, y: 68, w: 11, h: 13 }, '>', 'Next chaos trigger');
  const trigValue = centredLabel(doc, '', 86.5, 70.71);
  const stepTrigger = (dir: number) => set('chaosTrigger', (((get('chaosTrigger') + dir) % 4) + 4) % 4);
  prev.onClick = () => stepTrigger(-1);
  next.onClick = () => stepTrigger(1);
  const unitValue = centredLabel(doc, '', 90.5, 86.92, true);
  const unitText = label(doc, '', 21.04, 95.71);
  const unitLoop = new PixelSlider(doc, 62, 118, 100, scaleOf('chaosLoop'), 1, 'Chaos every n loops');
  unitLoop.onChange = ([v], g) => set('chaosLoop', v, g);
  const unitBeat = new PixelSlider(doc, 62, 118, 100, scaleOf('chaosBeat'), 1, 'Chaos every n beats');
  unitBeat.onChange = ([v], g) => set('chaosBeat', v, g);
  const divider = div(doc, 'ca-divider', { x: 125, y: 17, w: 1, h: 94 });
  chaos.append(picture, close.el, valueText, value.el, trigText, prev.el, trigValue, next.el, unitValue, unitText, unitLoop.el, unitBeat.el, divider);
  const groups: [string, string, number, number][] = [
    ['chaosShuffle', 'SHUFFLE', 17, 21.08],
    ['chaosLength', 'LENGTH', 36, 39.57],
    ['chaosSwing', 'SWING', 54, 58.07],
    ['chaosSustain', 'SUSTAIN', 73, 76.56],
    ['chaosChaos', 'CHAOS', 92, 95.05],
  ];
  const checks = groups.map(([id, text, boxY, textY]) => {
    const box = new PixelCheckbox(doc, 177, boxY, `Chaos changes ${text.toLowerCase()}`);
    box.onToggle = () => set(id, get(id) ? 0 : 1);
    chaos.append(label(doc, text, 132.87, textY, true), box.el);
    return { id, box };
  });

  const showPage = (page: 'main' | 'chaos') => {
    main.hidden = page !== 'main';
    chaos.hidden = page !== 'chaos';
  };
  openChaos.onClick = () => showPage('chaos');
  close.onClick = () => showPage('main');

  // ---- the device's silicone buttons: SHUFFLE and CHAOS ----
  const hardware = (id: 'shuffle' | 'chaos', x: number) => {
    const b = doc.createElement('button');
    b.type = 'button';
    b.className = 'ca-hw';
    b.setAttribute('aria-label', id === 'shuffle' ? 'Shuffle: new sequences' : 'Chaos: roll now');
    place(b, { x, y: 140, w: 74, h: 23 });
    const art = device.querySelector(`[id$="button-${id}"]`);
    const down = () => art?.classList.add('ca-down');
    const up = () => art?.classList.remove('ca-down');
    b.addEventListener('pointerdown', down);
    b.addEventListener('pointerup', up);
    b.addEventListener('pointerleave', up);
    b.addEventListener('click', () => press(id));
    return b;
  };

  root.append(main, chaos, hardware('shuffle', 11), hardware('chaos', 122));

  // ---- values → screen ----
  function refresh() {
    loopText.textContent = `LOOP (${pow2Text(get('loop'))})`;
    loop.set([get('loop')]);
    lenText.textContent = lengthText(get('noteLengthMin'), get('noteLengthMax'));
    len.set([get('noteLengthMin'), get('noteLengthMax')]);
    swing.set([get('swing')]);
    // Legato on: LEGATO, sustain unused. Off: SUSTAIN, its range on.
    const isLegato = get('legato') !== 0;
    legato.setOn(isLegato);
    legato.setText(isLegato ? 'LEGATO' : 'SUSTAIN');
    sustain.set([get('sustainMin'), get('sustainMax')]);
    sustain.setDisabled(isLegato);
    sustainText.textContent = `(${rangeText(get('sustainMin'), get('sustainMax'))})`;
    sustainText.classList.toggle('ca-dim', isLegato);
    retrig.setOn(get('retrig') !== 0);

    value.set([get('chaosValue')]);
    const trigger = get('chaosTrigger');
    trigValue.textContent = TRIGGERS[trigger] ?? '';
    // The unit row shows for the Beat and Loop triggers only.
    const isLoop = trigger === TRIGGER_LOOP;
    const isBeat = trigger === TRIGGER_BEAT;
    unitText.textContent = isLoop ? 'LOOP' : 'BEAT';
    unitValue.textContent = pow2Text(get(isLoop ? 'chaosLoop' : 'chaosBeat'));
    unitLoop.set([get('chaosLoop')]);
    unitBeat.set([get('chaosBeat')]);
    unitLoop.el.hidden = !isLoop;
    unitBeat.el.hidden = !isBeat;
    unitText.hidden = unitValue.hidden = !(isLoop || isBeat);
    for (const { id, box } of checks) box.set(get(id) !== 0);
  }
  refresh();
  cleanups.push(bridge.onParam(() => refresh()));

  // Keys the skin uses stay out of the DAW.
  root.addEventListener('keydown', (e) => {
    if (e.key.startsWith('Arrow') || e.key === ' ' || e.key === 'Enter') e.stopPropagation();
  });

  container.append(root);
  return () => {
    for (const c of cleanups) c();
    root.remove();
    releaseStyles();
  };
}
