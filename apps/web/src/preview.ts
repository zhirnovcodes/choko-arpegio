// Device preview (pnpm dev → /preview.html): the device artwork alone, at 1× and 3×. Buttons press.

import { deviceMarkup } from './ui/device/device';

const style = document.createElement('style');
style.textContent = `
  .ca-btn-top { transition: transform 40ms; }
  .ca-down .ca-btn-top { transform: translateY(2px); }
  .ca-down .ca-btn-shadow { opacity: 0.2; }
  [id$="button-shuffle"], [id$="button-chaos"] { cursor: pointer; }
`;
document.head.append(style);

for (const id of ['x1', 'x3']) {
  const host = document.getElementById(id)!;
  host.innerHTML = deviceMarkup();
  for (const button of host.querySelectorAll<SVGGElement>('[id$="button-shuffle"], [id$="button-chaos"]')) {
    button.addEventListener('pointerdown', () => button.classList.add('ca-down'));
    for (const up of ['pointerup', 'pointerleave']) button.addEventListener(up, () => button.classList.remove('ca-down'));
  }
}
