#include <SDL3/SDL.h>
#include <SDL3/SDL_opengles2.h>
#include <stdio.h>
#include <stdlib.h>

static const char *env_or_unset(const char *name) {
    const char *value = SDL_getenv(name);
    return value && value[0] ? value : "(unset)";
}

static void print_video_drivers(void) {
    const int count = SDL_GetNumVideoDrivers();
    printf("SDL video driver count=%d\n", count);
    for (int i = 0; i < count; ++i) {
        printf("SDL video driver[%d]=%s\n", i, SDL_GetVideoDriver(i));
    }
}

static void pump_events(int *running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            *running = 0;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            *running = 0;
        }
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN) {
            *running = 0;
        }
    }
}

int main(int argc, char **argv) {
    int frames = 300;
    if (argc > 1) {
        frames = atoi(argv[1]);
        if (frames <= 0) {
            frames = 300;
        }
    }

    printf("sdl2shim-gles-present-probe starting\n");
    printf("env SDL_VIDEODRIVER=%s\n", env_or_unset("SDL_VIDEODRIVER"));
    printf("env SDL3SHIM_SDL2_LIB=%s\n", env_or_unset("SDL3SHIM_SDL2_LIB"));
    printf("env SDL3SHIM_SDL2_VIDEODRIVER=%s\n", env_or_unset("SDL3SHIM_SDL2_VIDEODRIVER"));
    printf("env SDL_RENDER_DRIVER=%s\n", env_or_unset("SDL_RENDER_DRIVER"));
    printf("env SDL_VIDEO_FORCE_EGL=%s\n", env_or_unset("SDL_VIDEO_FORCE_EGL"));
    printf("env SDL_OPENGL_ES_DRIVER=%s\n", env_or_unset("SDL_OPENGL_ES_DRIVER"));
    print_video_drivers();

    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_SetHint(SDL_HINT_VIDEO_EGL_ALLOW_GETDISPLAY_FALLBACK, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }

    printf("SDL_Init ok current_video_driver=%s\n", SDL_GetCurrentVideoDriver());

    SDL_Window *window = SDL_CreateWindow("sdl2shim-gles-present-probe", 640, 480,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }

    int win_w = 0;
    int win_h = 0;
    int pixel_w = 0;
    int pixel_h = 0;
    SDL_GetWindowSize(window, &win_w, &win_h);
    SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
    printf("SDL_CreateWindow ok window=%dx%d pixels=%dx%d flags=0x%llx\n", win_w, win_h, pixel_w, pixel_h,
           (unsigned long long)SDL_GetWindowFlags(window));

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }

    if (!SDL_GL_MakeCurrent(window, context)) {
        printf("SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 5;
    }

    SDL_GL_SetSwapInterval(0);
    printf("SDL_GL_CreateContext ok context=%p\n", context);
    printf("GL_VENDOR=%s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER=%s\n", glGetString(GL_RENDERER));
    printf("GL_VERSION=%s\n", glGetString(GL_VERSION));
    printf("GL_SHADING_LANGUAGE_VERSION=%s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    int running = 1;
    for (int frame = 0; running && frame < frames; ++frame) {
        pump_events(&running);
        SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
        glViewport(0, 0, pixel_w, pixel_h);
        const float t = (float)(frame % 180) / 179.0f;
        const float r = frame < 60 ? 1.0f : (frame < 120 ? 0.0f : t);
        const float g = frame < 60 ? t : (frame < 120 ? 1.0f : 0.0f);
        const float b = frame < 60 ? 0.0f : (frame < 120 ? t : 1.0f);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
        if ((frame % 60) == 0) {
            const GLenum error = glGetError();
            printf("frame=%d pixels=%dx%d gl_error=0x%04x\n", frame, pixel_w, pixel_h, error);
            fflush(stdout);
        }
        SDL_Delay(16);
    }

    printf("present loop done\n");
    SDL_GL_MakeCurrent(window, NULL);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("sdl2shim-gles-present-probe done\n");
    return 0;
}
