import raw from '../../params.json';
import type { ParameterInfo } from 'ivandaw-core';

// params.json is the one parameter list. Array index = C ABI index = Engine::setParam index.
// `display: 'pow2'` params hold an exponent n meaning 2^n beats (or loops).
export type Param = ParameterInfo & { display?: 'pow2' };

export const PARAMS = raw.params as Param[];
export const INDEX = new Map(PARAMS.map((p, i) => [p.id, i]));

/** normalized 0..1 → plain min..max. Same function as the JUCE side. */
export function toPlain(p: ParameterInfo, n: number): number {
  const c = Math.min(1, Math.max(0, n));
  let v = p.scale === 'log' ? p.min * (p.max / p.min) ** c : p.min + c * (p.max - p.min);
  if (p.kind === 'int' || p.kind === 'choice' || p.kind === 'boolean' || p.kind === 'trigger') v = Math.round(v);
  return v;
}

export function toNormalized(p: ParameterInfo, v: number): number {
  const n = p.scale === 'log' ? Math.log(v / p.min) / Math.log(p.max / p.min) : (v - p.min) / (p.max - p.min);
  return Math.min(1, Math.max(0, n));
}
