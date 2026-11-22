# Choco Arpegio

A MIDI arpeggiator. Hold a chord; Choco Arpegio plays a generated, looping sequence of its notes, locked to the
host's beat. Note lengths, swing, sustain and note choice come from random sequences; **Shuffle** rolls new ones,
**Chaos** randomizes the settings themselves (by button, or on every note, beat or loop).

How it works: [docs/spec.md](docs/spec.md).

| Step | Scope | Status |
| --- | --- | --- |
| 1 | Repo, C++ core (sequences, time table, chaos, engine), core tests | done |
| 2 | Default UI, standalone web build with demo chords and a sine synth | done |
| 3 | Final design (device skin: plastic frame, LCD screen with main + chaos pages, silicone buttons) | done |
| 4 | CI/CD (release `daw-build-<n>` → `choko-arpegio-daw.zip`), IvanDAW integration | |
| 5 | VST3 / AU MIDI FX / Standalone (JUCE), Windows + Mac | |

## Build and test

Needs emscripten, CMake ≥ 3.25, Ninja, Node 24, pnpm 10.

```
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm
node build-wasm/ca_tests.js

pnpm install
pnpm --filter @choko-arpegio/web dev          # standalone page at http://localhost:5191
pnpm --filter @choko-arpegio/web build:web    # static site → apps/web/dist/
pnpm --filter @choko-arpegio/web build:daw    # IvanDAW plugin → apps/web/dist-daw/
```

The standalone page plays random 3–4 note chords (every beat … 2 bars, with or without a gap) into the
arpeggiator, which drives IvanDAW's base sine synth. Keys A…K hold notes yourself.
