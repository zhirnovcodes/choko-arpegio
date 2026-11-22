import fs from 'node:fs';
import type { Plugin } from 'vite';
import { defineConfig } from 'vitest/config';
import { alias, repoRoot, wasmCommand, wasmPath } from './vite.shared.ts';

// The standalone web version: index.html + dev.ts (host stand-in, demo chords, sine synth).
// `vite` serves it with the wasm the CMake build wrote; `vite build` writes it to dist/ for any static host.
function devWasm(): Plugin {
  return {
    name: 'choko-arpegio-dev-wasm',
    apply: 'serve',
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        if (req.url?.split('?')[0] !== '/choko-arpegio.wasm') {
          next();
          return;
        }
        if (!fs.existsSync(wasmPath)) {
          res.statusCode = 404;
          res.end(`build-wasm/choko-arpegio.wasm is missing. From the repo root:\n  ${wasmCommand}`);
          return;
        }
        res.setHeader('Content-Type', 'application/wasm');
        res.setHeader('Cache-Control', 'no-store');
        res.end(fs.readFileSync(wasmPath));
      });
    },
  };
}

function buildWasm(): Plugin {
  return {
    name: 'choko-arpegio-build-wasm',
    apply: 'build',
    buildStart() {
      if (!fs.existsSync(wasmPath)) {
        this.error(`build-wasm/choko-arpegio.wasm is missing. Run the wasm build first, from the repo root:\n  ${wasmCommand}`);
      }
    },
    generateBundle() {
      this.emitFile({ type: 'asset', fileName: 'choko-arpegio.wasm', source: fs.readFileSync(wasmPath) });
    },
  };
}

export default defineConfig({
  plugins: [devWasm(), buildWasm()],
  resolve: { alias },
  base: './',
  // localhost can be IPv6-only unless the host is set; listen on every loopback.
  server: { host: true, port: 5191, fs: { allow: [repoRoot] } },
  build: { outDir: 'dist', emptyOutDir: true, target: 'es2022' },
  worker: { format: 'es' },
  test: {
    include: ['src/**/*.test.ts'],
    environment: 'node',
  },
});
