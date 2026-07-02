#include <SDL3/SDL.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

typedef EGLDisplay (*PFN_eglGetPlatformDisplayEXT)(EGLenum platform, void *native_display,
                                                   const EGLint *attrib_list);

static void print_file_exists(const char *path) {
    printf("path %-24s %s\n", path, access(path, F_OK) == 0 ? "present" : "missing");
}

static void print_count(EGLDisplay display, const char *label, const EGLint *attribs) {
    EGLint count = 0;
    EGLBoolean ok = eglChooseConfig(display, attribs, NULL, 0, &count);
    printf("eglChooseConfig %-18s ok=%d count=%d error=0x%04x\n", label, ok, count, eglGetError());
}

static int probe_egl_gbm(void *gbm_device) {
    const char *client_exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("egl client extensions: %s\n", client_exts ? client_exts : "(null)");

    PFN_eglGetPlatformDisplayEXT get_platform_display =
        (PFN_eglGetPlatformDisplayEXT)eglGetProcAddress("eglGetPlatformDisplayEXT");
    printf("eglGetPlatformDisplayEXT=%p\n", (void *)get_platform_display);
    if (!get_platform_display) {
        return 20;
    }

    EGLDisplay display = get_platform_display(EGL_PLATFORM_GBM_KHR, gbm_device, NULL);
    printf("eglGetPlatformDisplayEXT(GBM, gbm_device)=%p error=0x%04x\n", (void *)display, eglGetError());
    if (display == EGL_NO_DISPLAY) {
        return 21;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(display, &major, &minor)) {
        printf("eglInitialize(GBM) failed error=0x%04x\n", eglGetError());
        return 22;
    }
    printf("eglInitialize(GBM) ok version=%d.%d vendor=%s\n", major, minor, eglQueryString(display, EGL_VENDOR));
    printf("egl display extensions: %s\n", eglQueryString(display, EGL_EXTENSIONS));

    const EGLint any_attribs[] = {EGL_NONE};
    const EGLint window_gles2_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };
    const EGLint rgba8888_window_gles2_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };
    const EGLint rgb565_window_gles2_attribs[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_BLUE_SIZE, 5,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };

    print_count(display, "any", any_attribs);
    print_count(display, "window_gles2", window_gles2_attribs);
    print_count(display, "rgba8888_window", rgba8888_window_gles2_attribs);
    print_count(display, "rgb565_window", rgb565_window_gles2_attribs);

    EGLConfig config = 0;
    EGLint count = 0;
    EGLBoolean chose =
        eglChooseConfig(display, rgba8888_window_gles2_attribs, &config, 1, &count);
    printf("eglChooseConfig selected_rgba8888 ok=%d count=%d config=%p error=0x%04x\n", chose, count,
           (void *)config, eglGetError());

    eglTerminate(display);
    return chose && count > 0 ? 0 : 23;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    setenv("SDL_VIDEODRIVER", "kmsdrm", 1);
    SDL_SetHint("SDL_KMSDRM_REQUIRE_DRM_MASTER", "0");

    printf("kmsdrm-egl-probe starting\n");
    printf("SDL_VIDEODRIVER=%s\n", SDL_getenv("SDL_VIDEODRIVER"));
    print_file_exists("/dev/dri/card0");
    print_file_exists("/dev/dri/renderD128");
    print_file_exists("/dev/fb0");
    print_file_exists("/dev/mali0");

    int driver_count = SDL_GetNumVideoDrivers();
    printf("SDL video driver count=%d\n", driver_count);
    for (int i = 0; i < driver_count; ++i) {
        printf("SDL video driver[%d]=%s\n", i, SDL_GetVideoDriver(i));
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
        return 2;
    }
    printf("SDL_Init ok current_video_driver=%s\n", SDL_GetCurrentVideoDriver());

    SDL_Window *window = SDL_CreateWindow("kmsdrm-egl-probe", 320, 240, SDL_WINDOW_FULLSCREEN);
    if (!window) {
        printf("SDL_CreateWindow(kmsdrm) failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }
    printf("SDL_CreateWindow ok\n");

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    Sint64 device_index = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_KMSDRM_DEVICE_INDEX_NUMBER, -1);
    Sint64 drm_fd = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_KMSDRM_DRM_FD_NUMBER, -1);
    void *gbm_device = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_KMSDRM_GBM_DEVICE_POINTER, NULL);
    printf("SDL KMSDRM properties: device_index=%lld drm_fd=%lld gbm_device=%p\n",
           (long long)device_index, (long long)drm_fd, gbm_device);

    int result = 10;
    if (drm_fd < 0 || gbm_device == NULL) {
        printf("missing SDL KMSDRM drm_fd or gbm_device\n");
        result = 4;
    } else {
        result = probe_egl_gbm(gbm_device);
    }

    printf("probe result=%d\n", result);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("kmsdrm-egl-probe done\n");
    return result;
}
