'use strict';
for (const button of document.querySelectorAll('[data-dialog]')) {
  button.addEventListener('click', () => document.getElementById(button.dataset.dialog).showModal());
}
for (const dialog of document.querySelectorAll('dialog')) {
  dialog.addEventListener('click', (event) => { if (event.target === dialog) {
    const r = dialog.getBoundingClientRect();
    if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) dialog.close();
  }});
  dialog.addEventListener('close', () => { if (document.body.classList.contains('playing')) document.getElementById('canvas').focus(); });
}
if (new URLSearchParams(location.search).has('netplay')) document.getElementById('go-online').classList.remove('hidden');
