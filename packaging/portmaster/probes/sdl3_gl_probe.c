#include <SDL3/SDL.h>
#include <stdio.h>

static void print_drivers(void) {
    const int count = SDL_GetNumVideoDrivers();
    printf("video_driver_count=%d\n", count);
    for (int i = 0; i < count; ++i) {
        printf("video_driver[%d]=%s\n", i, SDL_GetVideoDriver(i));
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("hint SDL_VIDEODRIVER=%s\n", SDL_getenv("SDL_VIDEODRIVER"));
    printf("hint SDL_VIDEO_FORCE_EGL=%s\n", SDL_getenv("SDL_VIDEO_FORCE_EGL"));
    printf("hint SDL_VIDEO_EGL_ALLOW_GETDISPLAY_FALLBACK=%s\n", SDL_getenv("SDL_VIDEO_EGL_ALLOW_GETDISPLAY_FALLBACK"));
    printf("hint SDL_OPENGL_ES_DRIVER=%s\n", SDL_getenv("SDL_OPENGL_ES_DRIVER"));
    print_drivers();

    const char* profile = SDL_getenv("PROBE_GL_PROFILE");
    if (profile && SDL_strcmp(profile, "desktop") == 0) {
        printf("probe_profile=desktop\n");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    } else {
        printf("probe_profile=opengles\n");
        SDL_SetHint(SDL_HINT_VIDEO_EGL_ALLOW_GETDISPLAY_FALLBACK, "1");
        SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }
    printf("current_video_driver=%s\n", SDL_GetCurrentVideoDriver());

    SDL_Window* window = SDL_CreateWindow("sdl3-gl-probe", 640, 480, SDL_WINDOW_OPENGL);
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }
    printf("SDL_CreateWindow ok\n");

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }
    printf("SDL_GL_CreateContext ok\n");

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
