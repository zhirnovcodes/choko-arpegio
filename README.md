# Choco Arpegio

A MIDI arpeggiator. Hold a chord; Choco Arpegio plays a generated, looping sequence of its notes, locked to the
host's beat. Note lengths, swing, sustain and note choice come from random sequences; **Shuffle** rolls new ones,
**Chaos** randomizes the settings themselves (by button, or on every note, beat or loop).

How it works: [docs/spec.md](docs/spec.md).

| Step | Scope | Status |
| --- | --- | --- |
| 1 | Repo, C++ core (sequences, time table, chaos, engine), core tests | done |
| 2 | Default UI, standalone web build with demo chords and a sine synth | |
| 3 | Final design | |
| 4 | VST3 / AU MIDI FX / Standalone (JUCE), Windows + Mac | |
| 5 | CI/CD, IvanDAW integration | |

## Build and test

Needs emscripten, CMake ≥ 3.25, Ninja, Node.

```
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm
node build-wasm/ca_tests.js
```
