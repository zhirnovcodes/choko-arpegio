// Value text for the skin. Pure functions; no DOM.

import type { ParameterInfo } from 'ivandaw-core';

export type SkinParam = ParameterInfo & { display?: 'pow2' };

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/** C3 = 60, as in IvanDAW and Ableton. */
export function noteName(pitch: number): string {
  return NOTE_NAMES[pitch % 12] + (Math.floor(pitch / 12) - 2);
}

/** 2^n as text: 4, 1, 1/8. */
export function pow2Text(n: number): string {
  const e = Math.round(n);
  return e >= 0 ? String(2 ** e) : `1/${2 ** -e}`;
}

/** The label of a plain value. */
export function formatValue(p: SkinParam, v: number): string {
  if (p.kind === 'choice' && p.choices) return p.choices[Math.round(v - p.min)] ?? String(v);
  if (p.kind === 'boolean') return v >= 0.5 ? 'On' : 'Off';
  if (p.kind === 'float') return v.toFixed(2);
  if (p.display === 'pow2') {
    const loops = p.unit?.includes('loop');
    const one = Math.round(v) === 0;
    return `${pow2Text(v)} ${loops ? (one ? 'loop' : 'loops') : one ? 'beat' : 'beats'}`;
  }
  return String(Math.round(v));
}
