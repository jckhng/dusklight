#include <SDL2/SDL.h>
#include <stdio.h>

static void print_video_drivers(void) {
    int count = SDL_GetNumVideoDrivers();
    printf("video_driver_count=%d\n", count);
    for (int i = 0; i < count; ++i) {
        printf("video_driver[%d]=%s\n", i, SDL_GetVideoDriver(i));
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *driver_hint = SDL_getenv("SDL_VIDEODRIVER");
    const char *force_egl = SDL_getenv("SDL_VIDEO_FORCE_EGL");
    const char *gles_driver = SDL_getenv("SDL_OPENGL_ES_DRIVER");
    printf("hint SDL_VIDEODRIVER=%s\n", driver_hint ? driver_hint : "(unset)");
    printf("hint SDL_VIDEO_FORCE_EGL=%s\n", force_egl ? force_egl : "(unset)");
    printf("hint SDL_OPENGL_ES_DRIVER=%s\n", gles_driver ? gles_driver : "(unset)");

    print_video_drivers();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("current_video_driver=%s\n", SDL_GetCurrentVideoDriver());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *window = SDL_CreateWindow(
        "sdl2_gles_probe",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640,
        480,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    int drawable_w = 0;
    int drawable_h = 0;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    printf("SDL2 GLES context created\n");
    printf("drawable=%dx%d\n", drawable_w, drawable_h);

    SDL_Delay(250);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
