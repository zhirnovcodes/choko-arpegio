# Choco Arpegio: rules for agents

Choco Arpegio is a MIDI arpeggiator / note generator: it takes a held MIDI chord and plays a generated
sequence of its notes, locked to the host's beat. Built from one C++ core:
an IvanDAW web plugin (wasm, Plugin API v2) and a JUCE plugin (VST3, AU MIDI FX, Standalone).

**Behaviour spec: [docs/spec.md](docs/spec.md). Read it before changing the engine, params or UI.**

Guide: ivan-daw `docs/plugin-dev/` (README, 01-architecture, types/midi-generator.md, 03-web-host, 04-vst-host, 05-ci-cd).
Sibling repos: `C:\Work\BananaChords` (same layout; its drift = our swing), `C:\Work\Beatality` (chaos model).

## Names

| Thing | Value |
| --- | --- |
| Repo / wasm / zip | `choko-arpegio` / `choko-arpegio.wasm` / `choko-arpegio-daw.zip` |
| IvanDAW id | `com.ivandaw.choko-arpegio` |
| JUCE target | `ChocoArpegio`, `ChocoArpegioMidiFx` |
| C ABI prefix / namespace | `ca_` / `ca::` |
| AU codes | manufacturer `Ivdw`, plugin `Ca01`, MIDI FX `Cax1` (check against Beatality / BananaChords before use) |

## Folder map

| Folder | Holds | Uses |
| --- | --- | --- |
| `core/domain/` | `Grid` (1/64-beat quanta), `Rng`, `Sequences` (times, swing, sustain, notes), `TimeTable`, `HeldNotes`, `Chaos` | C++20 std only; no exceptions, RTTI, I/O, allocation |
| `core/engine/` | `Engine`: params, transport, input chord, chaos, note output | core/domain |
| `tests/` | C++ test executable `ca_tests` | core |
| `params.json` | the only parameter list | both hosts, CMake (generates `ParamsJson.h`) |
| `chaos-ranges.json` | chaos ranges at chaos 0 / chaos 1 | CMake (into `ParamsJson.h`) |
| `hosts/web/`, `apps/web/` | step 2: C ABI, IvanDAW plugin, dev page | – |

## Commands

```
# core tests under Node (no native compiler needed)
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm
node build-wasm/ca_tests.js
# native core tests (CI, macOS / Linux)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --target ca_tests && ./build/ca_tests
```

## Rules

- All behaviour lives in `core/`. Hosts translate; they never decide musical behaviour.
- `params.json` is append-only. Ids and indexes never change. `enum Param` in `Engine.h` follows it.
- Pow-of-2 params are stored as integer exponents (`"display": "pow2"`): value n means 2^n beats (2^n loops for `chaosLoop`).
- Musical time in the core is in quanta (1/64 beat, `core/domain/Grid.h`). Notes change only on that grid or on input events.
- The same input gives the same output for any block size. Core tests check sizes 1, 64, 128, 512, 4096.
- Every note-on gets a note-off (stop, seek, CC 120/123, chord released, `allOff`). One output note at a time.
- Random sequences come from per-sequence seeds; state saves the seeds. Chaos uses its own RNG.
- Chaos changes params inside the engine; hosts read `Engine::takeChaosChanges()` after `process()` and push those values to the host.
- You cannot hear. Every check says exactly what Ivan should listen for.

## Workflow

- Commit and push after every implementation step (see docs/spec.md §7).
- Before pushing: `node build-wasm/ca_tests.js` passes.
