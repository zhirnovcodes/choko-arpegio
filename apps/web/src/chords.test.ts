import { describe, expect, it } from 'vitest';
import { NoteTimeline } from '../../../hosts/web/noteTimeline';
import { randomChord } from './chords';

describe('randomChord', () => {
  it('gives 3 or 4 ascending notes rooted in C2..B2, never the previous chord twice', () => {
    let previous: number[] = [];
    for (let i = 0; i < 500; i++) {
      const c = randomChord(Math.random, previous);
      expect(c.length === 3 || c.length === 4).toBe(true);
      expect(c[0]).toBeGreaterThanOrEqual(48);
      expect(c[0]).toBeLessThanOrEqual(59);
      for (let k = 1; k < c.length; k++) expect(c[k]).toBeGreaterThan(c[k - 1]);
      expect(c.join()).not.toBe(previous.join());
      previous = c;
    }
  });
});

describe('NoteTimeline', () => {
  it('answers what sounds at a time, from events booked ahead', () => {
    const t = new NoteTimeline();
    t.add(1, 60, true);
    t.add(1, 64, true);
    t.add(2, 60, false);
    t.add(3, 67, true);
    expect(t.at(0.5)).toEqual([]);
    expect(t.at(1)).toEqual([60, 64]);
    expect(t.at(2.5)).toEqual([64]);
    expect(t.at(3)).toEqual([64, 67]);
    // Old events are folded away without changing the answer.
    expect(t.at(10)).toEqual([64, 67]);
    t.clear(11);
    expect(t.at(11)).toEqual([]);
  });
});
