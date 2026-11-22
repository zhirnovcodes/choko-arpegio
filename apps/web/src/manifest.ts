import type { PluginManifest } from 'ivandaw-core';

// Plain data, so the DAW build can write it out as manifest.json without loading the plugin.
export const ID = 'com.ivandaw.choko-arpegio';
export const VERSION = '0.0.1'; // same as VERSION in CMakeLists.txt

export const manifest: PluginManifest = {
  id: ID,
  name: 'Choko Arpegio',
  vendor: 'zhirnovcodes',
  version: VERSION,
  apiVersion: 2,
  kind: 'midi',
  ports: [
    { id: 'in', label: 'Notes', kind: 'events', direction: 'in', accepts: ['midi', 'automation', 'tempo', 'transport'] },
    { id: 'out', label: 'Notes', kind: 'events', direction: 'out', accepts: ['midi'] },
  ],
  ui: { width: 600, height: 560, resizable: false },
  features: ['state'],
};
