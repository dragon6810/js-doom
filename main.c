#include <stdio.h>
#include <emscripten.h>
#include <SDL.h>
#include <stdint.h> // For uint32_t

// Global screen dimensions
const int gamewidth = 1280;
const int gameheight = 800;

SDL_Renderer *renderer;
SDL_Texture *screenTexture;
uint32_t *pixels; // Our raw pixel buffer

// Simple example for now: a moving colored rectangle
int rect_x = 10;
int rect_y = 10;
const int rect_size = 50;
const Uint8* keystates;

void main_loop()
{
    // Handle input
    SDL_PumpEvents();
    if (keystates[SDL_SCANCODE_LEFT]) rect_x -= 5;
    if (keystates[SDL_SCANCODE_RIGHT]) rect_x += 5;
    if (keystates[SDL_SCANCODE_UP]) rect_y -= 5;
    if (keystates[SDL_SCANCODE_DOWN]) rect_y += 5;

    // Clear screen (equivalent to pixels32.fill(0xFF000000);)
    for (int i = 0; i < gamewidth * gameheight; ++i) {
        pixels[i] = 0xFF000000; // Opaque Black (AARRGGBB - Little Endian)
    }

    // Draw a colored rectangle directly to the pixel buffer
    // Color: Opaque Red (FF0000FF - AARRGGBB)
    uint32_t red = 0xFFFF0000; // Example color
    for (int y = 0; y < rect_size; ++y) {
        for (int x = 0; x < rect_size; ++x) {
            int px = rect_x + x;
            int py = rect_y + y;
            if (px >= 0 && px < gamewidth && py >= 0 && py < gameheight) {
                pixels[py * gamewidth + px] = red;
            }
        }
    }

    // Update the texture with our pixel data
    SDL_UpdateTexture(screenTexture, NULL, pixels, gamewidth * sizeof(uint32_t));

    // Copy the texture to the renderer
    SDL_RenderCopy(renderer, screenTexture, NULL, NULL);

    // Present buffer
    SDL_RenderPresent(renderer);
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window;
    
    // Create an SDL window and renderer
    SDL_CreateWindowAndRenderer(gamewidth, gameheight, 0, &window, &renderer);

    // Create the texture that we will update with our raw pixels
    screenTexture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_ARGB8888, // AARRGGBB format (common for web)
                                      SDL_TEXTUREACCESS_STREAMING, // Allows frequent updates
                                      gamewidth, gameheight);

    // Allocate our pixel buffer
    pixels = (uint32_t*) malloc(gamewidth * gameheight * sizeof(uint32_t));
    if (pixels == NULL) {
        fprintf(stderr, "Failed to allocate pixel buffer!\n");
        return 1;
    }
    
    keystates = SDL_GetKeyboardState(NULL);

    // Start the main loop
    emscripten_set_main_loop(main_loop, 0, 1);

    // Cleanup (this part is technically unreachable in an infinite Emscripten loop)
    free(pixels);
    SDL_DestroyTexture(screenTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}