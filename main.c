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

double lastframetime, lastfpscheck;
int fpsframes;

void loop()
{
    double curtime, frametime;

    curtime = emscripten_get_now();
    frametime = (curtime - lastframetime) / 1000.0;

    if(curtime - lastfpscheck > 1000)
    {
        printf("%d fps\n", fpsframes);
        fpsframes = 0;
        lastfpscheck = curtime;
    }

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

    SDL_UpdateTexture(screenTexture, NULL, pixels, gamewidth * sizeof(uint32_t));

    SDL_RenderCopy(renderer, screenTexture, NULL, NULL);

    SDL_RenderPresent(renderer);

    lastframetime = curtime;
    fpsframes++;
}

int main()
{
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_CreateWindowAndRenderer(gamewidth, gameheight, 0, &window, &renderer);
    screenTexture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      gamewidth, gameheight);
    pixels = malloc(gamewidth * gameheight * sizeof(uint32_t));
    keystates = SDL_GetKeyboardState(NULL);

    lastframetime = lastfpscheck = emscripten_get_now();
    emscripten_set_main_loop(loop, 0, 1);

    free(pixels);
    SDL_DestroyTexture(screenTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}