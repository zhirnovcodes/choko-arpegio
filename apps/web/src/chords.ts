// Random 3–4 note chords for the standalone page: the input the arpeggiator plays from.

/** Intervals from the root. Triads, sevenths, sixths, sus. */
export const SHAPES: readonly (readonly number[])[] = [
  [0, 4, 7], // major
  [0, 3, 7], // minor
  [0, 3, 6], // diminished
  [0, 5, 7], // sus4
  [0, 2, 7], // sus2
  [0, 4, 7, 11], // maj7
  [0, 3, 7, 10], // m7
  [0, 4, 7, 10], // 7
  [0, 4, 7, 9], // 6
  [0, 3, 7, 9], // m6
  [0, 3, 6, 10], // m7b5
];

/** A chord rooted in C2..B2 (48..59), ascending, 3 or 4 notes. Never the same as `previous`. */
export function randomChord(rand: () => number = Math.random, previous: readonly number[] = []): number[] {
  for (let attempt = 0; attempt < 8; attempt++) {
    const root = 48 + Math.floor(rand() * 12);
    const shape = SHAPES[Math.floor(rand() * SHAPES.length)];
    const chord = shape.map((i) => root + i);
    if (chord.join() !== previous.join()) return chord;
  }
  return SHAPES[0].map((i) => 48 + i);
}
