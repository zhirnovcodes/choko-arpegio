// Which notes sound at a given AudioContext time, from note events booked ahead.
// The engine runs LOOKAHEAD ahead of the clock, so the UI asks "what sounds now" here.

export class NoteTimeline {
  private events: { time: number; pitch: number; on: boolean }[] = [];

  add(time: number, pitch: number, on: boolean) {
    // Kept in time order; equal times keep arrival order.
    let i = this.events.length;
    while (i > 0 && this.events[i - 1].time > time) i--;
    this.events.splice(i, 0, { time, pitch, on });
  }

  /** Everything stops at `time`. */
  clear(time: number) {
    for (const p of this.at(time)) this.add(time, p, false);
  }

  /** Pitches sounding at `time`, ascending. Forgets events long past. */
  at(time: number): number[] {
    const on = new Set<number>();
    let keepFrom = 0;
    for (let i = 0; i < this.events.length; i++) {
      const e = this.events[i];
      if (e.time > time) break;
      if (e.on) on.add(e.pitch);
      else on.delete(e.pitch);
      if (e.time < time - 1) keepFrom = i + 1;
    }
    if (keepFrom > 0) {
      // Fold the forgotten prefix into note-ons at its end, so the state stays the same.
      const folded = new Set<number>();
      for (const e of this.events.slice(0, keepFrom)) {
        if (e.on) folded.add(e.pitch);
        else folded.delete(e.pitch);
      }
      const t = this.events[keepFrom - 1].time;
      this.events = [...[...folded].map((pitch) => ({ time: t, pitch, on: true })), ...this.events.slice(keepFrom)];
    }
    return [...on].sort((a, b) => a - b);
  }
}
