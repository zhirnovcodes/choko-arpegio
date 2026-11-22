// The device artwork (device.svg): plastic frame, LCD screen, two silicone buttons, captions.

import plasticUrl from '../assets/plastic.jpg';
import deviceSvg from './device.svg?raw';

export const DEVICE = { width: 207, height: 169 } as const;

let instances = 0;

/** device.svg with the bundled texture URL, and its ids made unique: two plugin instances share a document. */
export function deviceMarkup(): string {
  const prefix = `ca${++instances}-`;
  return deviceSvg
    .replace('../assets/plastic.jpg', plasticUrl)
    .replace(/\bid="([^"]+)"/g, `id="${prefix}$1"`)
    .replace(/url\(#([^)]+)\)/g, `url(#${prefix}$1)`);
}
