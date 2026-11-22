import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { defineConfig, type Plugin } from 'vite';
import { manifest } from './src/manifest.ts';
import { alias, wasmCommand, wasmPath } from './vite.shared.ts';

// The IvanDAW build: one folder that works from any URL.
// index.js            the PluginFactory, skin and CSS bundled in
// choko-arpegio.wasm  the engine
// manifest.json       the manifest as data

function dawFiles(): Plugin {
  return {
    name: 'choko-arpegio-daw-files',
    buildStart() {
      if (!fs.existsSync(wasmPath)) {
        this.error(`build-wasm/choko-arpegio.wasm is missing. Run the wasm build first, from the repo root:\n  ${wasmCommand}`);
      }
    },
    generateBundle() {
      this.emitFile({ type: 'asset', fileName: 'choko-arpegio.wasm', source: fs.readFileSync(wasmPath) });
      this.emitFile({ type: 'asset', fileName: 'manifest.json', source: JSON.stringify(manifest, null, 2) + '\n' });
    },
    // index.js injects the CSS itself (skin.ts). Vite may still extract a copy.
    writeBundle(options) {
      for (const fileName of fs.readdirSync(options.dir!)) {
        if (fileName.endsWith('.css')) fs.rmSync(path.join(options.dir!, fileName));
      }
    },
  };
}

export default defineConfig({
  plugins: [dawFiles()],
  resolve: { alias },
  publicDir: false,
  build: {
    outDir: 'dist-daw',
    emptyOutDir: true,
    target: 'es2022',
    lib: {
      entry: fileURLToPath(new URL('../../hosts/web/plugin.ts', import.meta.url)),
      formats: ['es'],
      fileName: () => 'index.js',
    },
  },
});
