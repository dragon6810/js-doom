// This code is executed before the WASM module is loaded.
// It sets the canvas element that the SDL2 backend will use for rendering.
Module['canvas'] = (function() { 
    return document.getElementById('gamecanvas'); 
})();
