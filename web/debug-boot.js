// Debug boot script for WebAssembly builds
// Allows manual control over when the game starts

Module.onRuntimeInitialized = function() {
    console.log('Super Mango WebAssembly module loaded');
    console.log('Call Module.callMain() to start the game');
    
    // Auto-start after a short delay in debug mode
    setTimeout(function() {
        if (Module.callMain) {
            Module.callMain();
        }
    }, 100);
};
