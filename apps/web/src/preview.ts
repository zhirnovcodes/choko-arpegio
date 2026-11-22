// Device preview (pnpm dev → /preview.html): the device artwork alone, at 1× and 3×.

import device from './ui/device/device-base.svg?raw';

for (const id of ['x1', 'x3']) document.getElementById(id)!.innerHTML = device;
