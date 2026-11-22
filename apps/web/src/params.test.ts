import { describe, expect, it } from 'vitest';
import { INDEX, PARAMS, toNormalized, toPlain } from '../../../hosts/web/params';
import { formatValue, pow2Text } from './ui/format';

// params.json is the one parameter list. The core checks its length against enum Param (static_assert).

describe('params.json', () => {
  it('has unique ids', () => {
    const ids = PARAMS.map((p) => p.id);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('has well-formed ranges, defaults, and choices', () => {
    for (const p of PARAMS) {
      expect(p.min, p.id).toBeLessThan(p.max);
      expect(p.default, p.id).toBeGreaterThanOrEqual(p.min);
      expect(p.default, p.id).toBeLessThanOrEqual(p.max);
      if (p.kind === 'choice') expect(p.choices?.length, p.id).toBe(p.max - p.min + 1);
      else expect(p.choices, p.id).toBeUndefined();
      expect(typeof p.automatable, p.id).toBe('boolean');
      if (p.display === 'pow2') expect(p.kind, p.id).toBe('int');
    }
  });

  it('keeps the indexes the engine uses (append only)', () => {
    const order = [
      'loop', 'noteLengthMin', 'noteLengthMax', 'legato', 'sustainMin', 'sustainMax', 'swing', 'retrig',
      'shuffle', 'chaos', 'chaosValue', 'chaosTrigger', 'chaosLoop', 'chaosBeat',
      'chaosShuffle', 'chaosLength', 'chaosSwing', 'chaosSustain', 'chaosChaos', 'rng', 'chaosRng',
    ];
    expect(PARAMS.slice(0, order.length).map((p) => p.id)).toEqual(order);
    expect(INDEX.get('chaosChaos')).toBe(18);
    expect(INDEX.get('rng')).toBe(19);
    expect(INDEX.get('chaosRng')).toBe(20);
  });

  it('round-trips normalized values', () => {
    for (const p of PARAMS) {
      for (const n of [0, 0.25, 0.5, 1]) {
        const v = toPlain(p, n);
        expect(v, p.id).toBeGreaterThanOrEqual(p.min);
        expect(v, p.id).toBeLessThanOrEqual(p.max);
        expect(toPlain(p, toNormalized(p, v)), p.id).toBeCloseTo(v, 9);
      }
    }
  });

  it('shows pow2 params as beats and loops', () => {
    const p = (id: string) => PARAMS[INDEX.get(id)!];
    expect(pow2Text(-3)).toBe('1/8');
    expect(pow2Text(5)).toBe('32');
    expect(formatValue(p('loop'), 2)).toBe('4 beats');
    expect(formatValue(p('noteLengthMin'), 0)).toBe('1 beat');
    expect(formatValue(p('sustainMin'), -5)).toBe('1/32 beats');
    expect(formatValue(p('chaosLoop'), 0)).toBe('1 loop');
    expect(formatValue(p('chaosLoop'), -2)).toBe('1/4 loops');
    expect(formatValue(p('chaosTrigger'), 2)).toBe('Beat');
    expect(formatValue(p('legato'), 1)).toBe('On');
  });
});
