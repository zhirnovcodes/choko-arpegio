// Skin preview (pnpm dev → /preview.html): the UI alone on the real engine (wasm), no audio, at 1× and 3×.
// Query: any param id = plain value (e.g. ?legato=1&chaosTrigger=3); &page=chaos opens the chaos page.

import { loadCore } from '../../../hosts/web/core';
import { INDEX, PARAMS, toNormalized, toPlain } from '../../../hosts/web/params';
import { mountSkin, type SkinBridge } from './ui/skin';

const query = new URLSearchParams(location.search);
const core = await loadCore(new URL('choko-arpegio.wasm', document.baseURI));
const engine = core.ca_create(48000, 1);
const plain = (id: string) => core.ca_param(engine, INDEX.get(id)!);
for (const p of PARAMS) if (query.has(p.id)) core.ca_set_param(engine, INDEX.get(p.id)!, Number(query.get(p.id)));

const listeners = new Set<(id: string, n: number) => void>();
const bridge: SkinBridge = {
  params: PARAMS,
  get: (id) => toNormalized(PARAMS[INDEX.get(id)!], plain(id)),
  set(id, n) {
    const index = INDEX.get(id)!;
    const p = PARAMS[index];
    const before = PARAMS.map((q) => plain(q.id));
    if (p.kind === 'trigger') {
      if (n > 0.5) {
        core.ca_set_param(engine, index, 1);
        core.ca_set_param(engine, index, 0);
      }
    } else {
      core.ca_set_param(engine, index, toPlain(p, n));
    }
    // As the plugin host: the skin is not told about its own change, only about params the engine moved
    // (the Chaos button). So the two previews do not keep each other in step for direct edits.
    PARAMS.forEach((q, i) => {
      if (i !== index && plain(q.id) !== before[i]) for (const l of [...listeners]) l(q.id, bridge.get(q.id));
    });
  },
  onParam(cb) {
    listeners.add(cb);
    return () => listeners.delete(cb);
  },
  view: () => ({ loop: 4, origin: 0, slots: [] }),
  onView: () => () => {},
  playhead: () => null,
  held: () => [],
  played: () => [],
};

for (const id of ['x1', 'x3']) {
  const host = document.getElementById(id)!;
  host.innerHTML = '';
  mountSkin(host, bridge);
  if (query.get('page') === 'chaos') (host.querySelector('[aria-label="Chaos settings"]') as HTMLElement).click();
}
