import { describe, expect, it } from 'vitest';
import { lengthText, sustainText } from './skin';

// The LEN row has 11 characters before its slider (design: "LEN. (32-32)" at 7.29 px per character).
describe('lengthText', () => {
  it('uses the design form when it fits, shorter forms when not, never over 11 characters', () => {
    expect(lengthText(1, 2)).toBe('LEN. (2-4)');
    expect(lengthText(-2, -1)).toBe('LEN 1/4-1/2');
    expect(lengthText(-4, -1)).toBe('LEN /16-/2');
    for (let a = -4; a <= 4; a++) for (let b = a; b <= 4; b++) expect(lengthText(a, b).length).toBeLessThanOrEqual(11);
  });
});

// The SUS row has 13 characters before its slider (design: "SUS (1/8-1/4)").
describe('sustainText', () => {
  it('uses the design form when it fits, never over 13 characters', () => {
    expect(sustainText(-3, -2)).toBe('SUS (1/8-1/4)');
    expect(sustainText(-4, -1)).toBe('SUS 1/16-1/2');
    for (let a = -5; a <= 4; a++) for (let b = a; b <= 4; b++) expect(sustainText(a, b).length).toBeLessThanOrEqual(13);
  });
});
