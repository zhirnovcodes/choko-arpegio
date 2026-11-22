import { fileURLToPath } from 'node:url';

// Shared by the dev page, the standalone web build, Vitest and the DAW build.

export const repoRoot = fileURLToPath(new URL('../..', import.meta.url));
export const wasmPath = fileURLToPath(new URL('../../build-wasm/choko-arpegio.wasm', import.meta.url));
export const wasmCommand =
  'emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-wasm --target choko-arpegio';

// hosts/web/ sits outside this package, so a bare 'ivandaw-core' import there would not find
// apps/web/node_modules. Point it at this package's copy.
export const alias = {
  'ivandaw-core': fileURLToPath(new URL('./node_modules/ivandaw-core/dist/index.js', import.meta.url)),
};
