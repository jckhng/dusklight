#include <SDL3/SDL.h>
#include <stdio.h>

int main(void) {
    printf("hint SDL_VIDEODRIVER=%s\n", SDL_getenv("SDL_VIDEODRIVER"));
    const int count = SDL_GetNumVideoDrivers();
    printf("video_driver_count=%d\n", count);
    for (int i = 0; i < count; ++i) {
        printf("video_driver[%d]=%s\n", i, SDL_GetVideoDriver(i));
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }
    printf("current_video_driver=%s\n", SDL_GetCurrentVideoDriver());
    SDL_Window *window = SDL_CreateWindow("sdl3-window-probe", 640, 480, 0);
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }
    printf("SDL_CreateWindow ok\n");
    SDL_Delay(500);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
