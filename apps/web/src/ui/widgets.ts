// The screens' building blocks at design size. Every coordinate is px on the 207 x 169 device
// (Frame 2.svg with the device origin subtracted: main page −46,−32, chaos page −272,−32).

export interface Box {
  x: number;
  y: number;
  w?: number;
  h?: number;
}

export function place<T extends HTMLElement>(e: T, b: Box): T {
  e.style.left = `${b.x}px`;
  e.style.top = `${b.y}px`;
  if (b.w !== undefined) e.style.width = `${b.w}px`;
  if (b.h !== undefined) e.style.height = `${b.h}px`;
  return e;
}

export function div(doc: Document, cls: string, b?: Box, text?: string): HTMLDivElement {
  const e = doc.createElement('div');
  e.className = cls;
  if (b) place(e, b);
  if (text !== undefined) e.textContent = text;
  return e;
}

// 8-bit Operator+ 8 at 12.5 px (one font pixel = 1.0417 px, as in the design). In its 15 px line box a glyph
// starts 1 px right and 3.1 px down, so a label placed at a design glyph's top-left lands on it.
// The small text (chaos page checkboxes) is the same font at one font pixel = 0.868 px: 10.42 px.
const SMALL = 0.868 / 1.0417;
const GLYPH_DX = 1;
const GLYPH_DY = 3.1;

/** A text whose first glyph's top-left is at (x, y), as read from the design. */
export function label(doc: Document, text: string, x: number, y: number, small = false): HTMLDivElement {
  const k = small ? SMALL : 1;
  const e = div(doc, small ? 'ca-text ca-small' : 'ca-text', { x: x - GLYPH_DX * k, y: y - GLYPH_DY * k }, text);
  return e;
}

/** A text centred on cx; y is the glyph top. */
export function centredLabel(doc: Document, text: string, cx: number, y: number, small = false): HTMLDivElement {
  const e = label(doc, text, cx, y, small);
  e.classList.add('ca-center');
  e.style.left = `${cx}px`;
  return e;
}

/** A grey pixel button: #D9DEE2 face, 1 px #6E757C shadow down-right; pressed (or on) sits on its shadow. */
export class PixelButton {
  readonly el: HTMLButtonElement;
  private readonly text: HTMLSpanElement;
  onClick: () => void = () => {};

  constructor(doc: Document, face: Box & { w: number; h: number }, text: string, ariaLabel: string) {
    const e = doc.createElement('button');
    e.type = 'button';
    e.className = 'ca-btn';
    e.setAttribute('aria-label', ariaLabel);
    place(e, face);
    this.text = doc.createElement('span');
    this.text.className = 'ca-btn-text';
    this.text.textContent = text;
    e.append(this.text);
    e.addEventListener('click', () => this.onClick());
    this.el = e;
  }

  setText(text: string) {
    this.text.textContent = text;
  }

  /** On: drawn pressed (toggles). */
  setOn(on: boolean) {
    this.el.classList.toggle('ca-on', on);
    this.el.setAttribute('aria-pressed', String(on));
  }
}

/** Integer steps or a float in [min, max]. */
export interface Scale {
  min: number;
  max: number;
  step: number; // 0 = continuous
}

const KNOB_W = 6;
const KNOB_H = 10;

function quantize(s: Scale, v: number): number {
  const c = Math.min(s.max, Math.max(s.min, v));
  return s.step > 0 ? Math.round((c - s.min) / s.step) * s.step + s.min : c;
}

/**
 * A pixel slider: a 2 px black line from x0 to x1 at y, and one or two 6 x 10 knobs (#D9DEE2 with a #6E757C
 * shadow down-right). Two knobs make a range: the knob nearest the pointer moves, and it pushes the other along.
 */
export class PixelSlider {
  readonly el: HTMLDivElement;
  private readonly knobs: HTMLDivElement[] = [];
  private values: number[];
  private disabled = false;
  /** Called while dragging with the new values (one or two). */
  onChange: (values: number[], gesture?: 'begin' | 'end') => void = () => {};

  constructor(
    doc: Document,
    private readonly x0: number,
    private readonly x1: number,
    y: number,
    private readonly scale: Scale,
    knobCount: 1 | 2,
    ariaLabel: string,
  ) {
    // Hit area: the line plus the knobs' height.
    const el = div(doc, 'ca-slider', { x: x0, y: y - KNOB_H / 2 - 1, w: x1 - x0, h: KNOB_H + 2 });
    el.tabIndex = 0;
    el.setAttribute('role', 'slider');
    el.setAttribute('aria-label', ariaLabel);
    el.append(div(doc, 'ca-line', { x: 0, y: KNOB_H / 2, w: x1 - x0, h: 2 }));
    for (let i = 0; i < knobCount; i++) {
      const k = div(doc, 'ca-knob', { x: 0, y: 0, w: KNOB_W, h: KNOB_H });
      el.append(k);
      this.knobs.push(k);
    }
    this.values = Array.from({ length: knobCount }, () => scale.min);
    this.el = el;
    this.listen();
  }

  set(values: number[]) {
    this.values = values.map((v) => quantize(this.scale, v));
    this.knobs.forEach((k, i) => {
      const t = (this.values[i] - this.scale.min) / (this.scale.max - this.scale.min);
      k.style.left = `${Math.round(t * (this.x1 - this.x0 - KNOB_W))}px`;
    });
    this.el.setAttribute('aria-valuetext', this.values.join(' – '));
  }

  setDisabled(off: boolean) {
    this.disabled = off;
    this.el.classList.toggle('ca-disabled', off);
    this.el.setAttribute('aria-disabled', String(off));
  }

  private valueAt(clientX: number): number {
    const r = this.el.getBoundingClientRect();
    const t = (clientX - r.left) / r.width; // works under CSS zoom / transforms: both sides scale
    const px = t * (this.x1 - this.x0) - KNOB_W / 2;
    return quantize(this.scale, this.scale.min + (px / (this.x1 - this.x0 - KNOB_W)) * (this.scale.max - this.scale.min));
  }

  private move(index: number, v: number, gesture?: 'begin' | 'end') {
    const next = [...this.values];
    next[index] = v;
    // A range: min pushes max along, max pushes min.
    if (next.length === 2) {
      if (index === 0 && next[0] > next[1]) next[1] = next[0];
      if (index === 1 && next[1] < next[0]) next[0] = next[1];
    }
    this.set(next);
    this.onChange(this.values, gesture);
  }

  private listen() {
    let active = -1;
    this.el.addEventListener('pointerdown', (e) => {
      if (this.disabled || e.button !== 0) return;
      e.preventDefault();
      this.el.focus();
      const v = this.valueAt(e.clientX);
      // Nearest knob; on a tie (stacked knobs) the one the pointer moves towards decides on the next move.
      active = 0;
      if (this.values.length === 2) {
        const d0 = Math.abs(v - this.values[0]);
        const d1 = Math.abs(v - this.values[1]);
        active = d1 < d0 || (d1 === d0 && v > this.values[1]) ? 1 : 0;
      }
      this.el.setPointerCapture(e.pointerId);
      this.move(active, v, 'begin');
    });
    this.el.addEventListener('pointermove', (e) => {
      if (active < 0) return;
      this.move(active, this.valueAt(e.clientX));
    });
    const end = (e: PointerEvent) => {
      if (active < 0) return;
      this.move(active, this.valueAt(e.clientX), 'end');
      active = -1;
    };
    this.el.addEventListener('pointerup', end);
    this.el.addEventListener('pointercancel', end);
    // Arrows move the (last) knob one step; Shift + arrows move the first knob of a range.
    this.el.addEventListener('keydown', (e) => {
      if (this.disabled) return;
      const dir = e.key === 'ArrowRight' || e.key === 'ArrowUp' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowDown' ? -1 : 0;
      if (!dir) return;
      e.preventDefault();
      e.stopPropagation();
      const index = this.values.length === 2 && !e.shiftKey ? 1 : 0;
      const step = this.scale.step > 0 ? this.scale.step : (this.scale.max - this.scale.min) / 50;
      this.move(index, this.values[index] + dir * step);
    });
  }
}

/** A 13 x 13 box with a 1 px black outline on #D9DEE2; on shows a small pixel ring. */
export class PixelCheckbox {
  readonly el: HTMLButtonElement;
  onToggle: () => void = () => {};

  constructor(doc: Document, x: number, y: number, ariaLabel: string) {
    const e = doc.createElement('button');
    e.type = 'button';
    e.className = 'ca-check';
    e.setAttribute('role', 'checkbox');
    e.setAttribute('aria-label', ariaLabel);
    place(e, { x, y, w: 14, h: 14 });
    // The ring: 5 x 5 font pixels (0.868 px) with the corners missing, as in the design.
    e.innerHTML =
      '<svg class="ca-ring" width="4.34" height="4.34" viewBox="0 0 5 5"><path d="M1 0H4V1H1Z M0 1H1V4H0Z M4 1H5V4H4Z M1 4H4V5H1Z" fill="#000"/></svg>';
    e.addEventListener('click', () => this.onToggle());
    this.el = e;
  }

  set(on: boolean) {
    this.el.classList.toggle('ca-on', on);
    this.el.setAttribute('aria-checked', String(on));
  }
}
