// Debug page marker. tools/web_debug_page.py inlines this into
// super-mango-debug.html, just before the shared super-mango.js, so both
// pages run the same .wasm/.data. Boot remains owned by web/boot.js, which
// reads this marker when the runtime is ready.
// Never install a second callback or call Module.callMain twice.
window.Module.__superMangoDebug = true;
