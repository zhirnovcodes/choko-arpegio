// The Choko Arpegio skin (default style): plain DOM, used by every host through SkinBridge.
// It never knows which host runs it. All classes start with "ca-".

import css from './skin.css?inline';
import { formatValue, noteName, pow2Text, type SkinParam } from './format';

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
    element.dataset.chokoArpegio = '';
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

const plainOf = (p: SkinParam, n: number) => {
  const v = p.min + Math.min(1, Math.max(0, n)) * (p.max - p.min);
  return p.kind === 'float' ? v : Math.round(v);
};
const normOf = (p: SkinParam, v: number) => (v - p.min) / (p.max - p.min);

function el<K extends keyof HTMLElementTagNameMap>(doc: Document, tag: K, cls?: string, text?: string) {
  const e = doc.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

// Chaos Loop shows only for the Loop trigger, Chaos Beat only for Beat. Sustain is off with Legato.
const TRIGGER_BEAT = 2;
const TRIGGER_LOOP = 3;

/** Renders the skin into `container`. Returns the unmount function. */
export function mountSkin(container: HTMLElement, bridge: SkinBridge): () => void {
  const doc = container.ownerDocument;
  const win = doc.defaultView ?? window;
  const releaseStyles = holdStyles(doc);
  const cleanups: (() => void)[] = [];
  const byId = new Map(bridge.params.map((p) => [p.id, p]));
  const plain = (id: string) => plainOf(byId.get(id)!, bridge.get(id));

  const root = el(doc, 'div', 'ca-skin');

  // Header: title and the two buttons.
  const header = el(doc, 'div', 'ca-header');
  const title = el(doc, 'div', 'ca-title');
  title.append(el(doc, 'span', 'ca-logo', 'CHOKO'), el(doc, 'span', 'ca-logo2', 'ARPEGIO'));
  const buttons = el(doc, 'div', 'ca-buttons');
  for (const p of bridge.params.filter((q) => q.kind === 'trigger')) {
    const b = el(doc, 'button', 'ca-button' + (p.id === 'chaos' ? ' ca-chaos' : ''), p.label);
    b.type = 'button';
    b.dataset.param = p.id;
    b.title = p.id === 'shuffle' ? 'New sequences' : 'Roll chaos now';
    b.addEventListener('click', () => {
      bridge.set(p.id, 1, 'begin');
      bridge.set(p.id, 0, 'end');
    });
    buttons.append(b);
  }
  header.append(title, buttons);

  // Timeline: one block per slot; inside it the sounding part as a bar at the height of its note pick.
  const timeline = el(doc, 'div', 'ca-timeline');
  const blocks = el(doc, 'div', 'ca-blocks');
  const head = el(doc, 'div', 'ca-playhead');
  const ruler = el(doc, 'div', 'ca-ruler');
  timeline.append(blocks, head);
  let current: SequenceView = bridge.view();
  let blockEls: HTMLElement[] = [];

  const renderView = () => {
    current = bridge.view();
    const loop = current.loop;
    blocks.replaceChildren();
    ruler.replaceChildren();
    blockEls = current.slots.map((s, i) => {
      const b = el(doc, 'div', 'ca-block' + (i % 2 ? ' ca-alt' : ''));
      b.style.left = `${(s.start / loop) * 100}%`;
      b.style.width = `${((s.end - s.start) / loop) * 100}%`;
      const bar = el(doc, 'div', 'ca-bar');
      bar.style.width = `${((s.soundEnd - s.start) / (s.end - s.start)) * 100}%`;
      bar.style.bottom = `${8 + s.value * 76}%`;
      b.append(bar);
      b.title = `Note ${i + 1}: from beat ${+s.start.toFixed(3)}, sounds ${+(s.soundEnd - s.start).toFixed(3)} of ${+(s.end - s.start).toFixed(3)} beats, pick ${s.value.toFixed(2)}`;
      blocks.append(b);
      return b;
    });
    // Ruler: a tick per beat (per bar from 16 beats, per quarter of the loop below 1 beat), labelled by beat.
    const step = loop >= 16 ? 4 : loop >= 1 ? 1 : loop / 4;
    for (let beat = 0; beat < loop - 1e-9; beat += step) {
      const tick = el(doc, 'div', 'ca-tick', step >= 1 ? String(beat + 1) : '');
      tick.style.left = `${(beat / loop) * 100}%`;
      ruler.append(tick);
    }
    if (loop < 1) ruler.append(el(doc, 'div', 'ca-ruler-note', `loop ${pow2Text(Math.log2(loop))} beat`));
  };
  renderView();
  cleanups.push(bridge.onView(renderView));

  // Notes: what is held and what plays.
  const notesRow = el(doc, 'div', 'ca-notes');
  const heldEl = el(doc, 'span', 'ca-held');
  const playedEl = el(doc, 'span', 'ca-played');
  notesRow.append(el(doc, 'span', 'ca-notes-label', 'Chord'), heldEl, el(doc, 'span', 'ca-notes-label', 'Plays'), playedEl);
  let lastHeld = '';
  let lastPlayed = '';

  let frame = 0;
  let lastActive = -1;
  const animate = () => {
    const beat = bridge.playhead();
    let active = -1;
    const loop = current.loop;
    if (beat === null || loop <= 0) {
      head.style.display = 'none';
    } else {
      const pos = ((((beat - current.origin) % loop) + loop) % loop);
      head.style.display = '';
      head.style.left = `${(pos / loop) * 100}%`;
      active = current.slots.findIndex((s) => pos >= s.start && pos < s.end);
    }
    if (active !== lastActive) {
      blockEls[lastActive]?.classList.remove('ca-active');
      blockEls[active]?.classList.add('ca-active');
      lastActive = active;
    }
    const held = bridge.held();
    const played = bridge.played();
    const heldText = held.length ? held.map(noteName).join(' ') : '—';
    const playedText = played.length ? played.map(noteName).join(' ') : '—';
    if (heldText !== lastHeld) heldEl.textContent = lastHeld = heldText;
    if (playedText !== lastPlayed) playedEl.textContent = lastPlayed = playedText;
    frame = win.requestAnimationFrame(animate);
  };
  frame = win.requestAnimationFrame(animate);
  cleanups.push(() => win.cancelAnimationFrame(frame));

  // Controls, by group.
  const groups = el(doc, 'div', 'ca-groups');
  const updaters = new Map<string, () => void>();
  const rows = new Map<string, HTMLElement>();
  const groupEls = new Map<string, HTMLElement>();
  for (const p of bridge.params) {
    if (p.kind === 'trigger') continue;
    const group = p.group ?? 'Main';
    let g = groupEls.get(group);
    if (!g) {
      g = el(doc, 'div', 'ca-group');
      g.dataset.group = group;
      g.append(el(doc, 'div', 'ca-group-title', group));
      groupEls.set(group, g);
      groups.append(g);
    }
    const row = el(doc, 'label', 'ca-row');
    row.dataset.param = p.id;
    const label = el(doc, 'span', 'ca-label', p.label);
    const value = el(doc, 'span', 'ca-value');
    let input: HTMLInputElement | HTMLSelectElement;
    if (p.kind === 'choice' && p.choices) {
      const select = el(doc, 'select', 'ca-select');
      p.choices.forEach((c, i) => {
        const o = el(doc, 'option', undefined, c);
        o.value = String(p.min + i);
        select.append(o);
      });
      select.addEventListener('change', () => {
        bridge.set(p.id, normOf(p, Number(select.value)));
        refreshVisibility();
      });
      input = select;
      updaters.set(p.id, () => {
        select.value = String(plainOf(p, bridge.get(p.id)));
      });
    } else if (p.kind === 'boolean') {
      const box = el(doc, 'input', 'ca-toggle');
      box.type = 'checkbox';
      box.addEventListener('change', () => {
        bridge.set(p.id, box.checked ? 1 : 0);
        refreshVisibility();
      });
      input = box;
      updaters.set(p.id, () => {
        box.checked = bridge.get(p.id) >= 0.5;
      });
    } else {
      const range = el(doc, 'input', 'ca-range');
      range.type = 'range';
      range.min = String(p.min);
      range.max = String(p.max);
      range.step = p.kind === 'float' ? '0.01' : '1';
      let dragging = false;
      range.addEventListener('pointerdown', () => {
        dragging = true;
        bridge.set(p.id, normOf(p, Number(range.value)), 'begin');
      });
      range.addEventListener('input', () => {
        bridge.set(p.id, normOf(p, Number(range.value)));
        value.textContent = formatValue(p, Number(range.value));
      });
      range.addEventListener('change', () => {
        bridge.set(p.id, normOf(p, Number(range.value)), dragging ? 'end' : undefined);
        dragging = false;
      });
      // Arrow keys move the slider; keep them from the DAW.
      range.addEventListener('keydown', (e) => {
        if (e.key.startsWith('Arrow') || e.key === 'Home' || e.key === 'End' || e.key.startsWith('Page')) e.stopPropagation();
      });
      input = range;
      updaters.set(p.id, () => {
        const v = plainOf(p, bridge.get(p.id));
        range.value = String(v);
        value.textContent = formatValue(p, v);
      });
    }
    input.dataset.param = p.id;
    row.append(label, input);
    if (p.kind !== 'choice' && p.kind !== 'boolean') row.append(value);
    if (p.kind === 'boolean') row.classList.add('ca-row-toggle');
    rows.set(p.id, row);
    g.append(row);
  }

  const setDisabled = (id: string, off: boolean) => {
    const row = rows.get(id);
    if (!row) return;
    row.classList.toggle('ca-disabled', off);
    const input = row.querySelector('input, select') as HTMLInputElement | null;
    if (input) input.disabled = off;
  };
  const setHidden = (id: string, hidden: boolean) => rows.get(id)?.classList.toggle('ca-hidden', hidden);
  function refreshVisibility() {
    const legato = plain('legato') >= 0.5;
    setDisabled('sustainMin', legato);
    setDisabled('sustainMax', legato);
    const trigger = plain('chaosTrigger');
    setHidden('chaosLoop', trigger !== TRIGGER_LOOP);
    setHidden('chaosBeat', trigger !== TRIGGER_BEAT);
  }

  const refresh = (id: string) => {
    updaters.get(id)?.();
    refreshVisibility();
  };
  for (const id of updaters.keys()) updaters.get(id)?.();
  refreshVisibility();
  cleanups.push(bridge.onParam((id) => refresh(id)));

  root.append(header, timeline, ruler, notesRow, groups);
  container.append(root);

  return () => {
    for (const c of cleanups) c();
    root.remove();
    releaseStyles();
  };
}
