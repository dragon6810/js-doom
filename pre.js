// This code is executed before the WASM module is loaded.
// It sets the canvas element that the SDL2 backend will use for rendering.

const RENDER_WIDTH = 1280;
const RENDER_HEIGHT = 800;

Module['canvas'] = (function() { 
    const canvas = document.getElementById('gamecanvas');
    // Set the physical size of the canvas to fill the window
    canvas.width = RENDER_WIDTH;
    canvas.height = RENDER_HEIGHT;
    return canvas; 
})();

Module.onRuntimeInitialized = function() {
    // Set the logical, internal rendering resolution.
    // SDL will automatically scale this up to the canvas size.
    Module._screen_init(RENDER_WIDTH, RENDER_HEIGHT);
};