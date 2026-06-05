#define LINUX 1
#define EGL_API_FB 1

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

struct fbdev_window {
    unsigned short width;
    unsigned short height;
};

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

static int env_int(const char *name, int fallback) {
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    return atoi(value);
}

static void print_gl_info(void) {
    printf("gl_vendor=%s\n", glGetString(GL_VENDOR));
    printf("gl_renderer=%s\n", glGetString(GL_RENDERER));
    printf("gl_version=%s\n", glGetString(GL_VERSION));
}

int main(void) {
    int fd = open("/dev/fb0", O_RDWR, 0);
    struct fb_var_screeninfo vinfo;
    if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("fb");
        if (fd >= 0) {
            close(fd);
        }
        return 1;
    }
    close(fd);

    const int run_seconds = env_int("PROBE_SECONDS", 20);
    const int swap_interval = env_int("PROBE_SWAP_INTERVAL", 0);
    const int width = env_int("PROBE_WIDTH", (int)vinfo.xres);
    const int height = env_int("PROBE_HEIGHT", (int)vinfo.yres);
    struct fbdev_window window = {
        .width = (unsigned short)width,
        .height = (unsigned short)height,
    };
    printf("fb=%ux%u bpp=%u virtual=%ux%u probe=%dx%d seconds=%d swap_interval=%d\n",
           vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.xres_virtual, vinfo.yres_virtual,
           width, height, run_seconds, swap_interval);

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint egl_major = 0;
    EGLint egl_minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &egl_major, &egl_minor)) {
        printf("egl init failed display=%p error=0x%04x\n", (void *)display, eglGetError());
        return 2;
    }
    printf("egl=%d.%d vendor=%s client_apis=%s\n", egl_major, egl_minor,
           eglQueryString(display, EGL_VENDOR), eglQueryString(display, EGL_CLIENT_APIS));

    const EGLint config_attribs[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_BLUE_SIZE, 5,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };
    EGLConfig config = 0;
    EGLint count = 0;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &count) || count < 1) {
        printf("choose_config failed count=%d error=0x%04x\n", count, eglGetError());
        eglTerminate(display);
        return 3;
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)&window, NULL);
    printf("surface=%p error=0x%04x\n", (void *)surface, eglGetError());
    if (surface == EGL_NO_SURFACE) {
        eglTerminate(display);
        return 4;
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        printf("eglBindAPI failed error=0x%04x\n", eglGetError());
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return 5;
    }

    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    printf("context=%p error=0x%04x\n", (void *)context, eglGetError());
    if (context == EGL_NO_CONTEXT) {
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return 6;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        printf("eglMakeCurrent failed error=0x%04x\n", eglGetError());
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return 7;
    }
    eglSwapInterval(display, swap_interval);
    print_gl_info();

    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    uint64_t start = now_us();
    uint64_t interval_start = start;
    uint64_t total_swap_us = 0;
    uint64_t max_swap_us = 0;
    uint64_t interval_swap_us = 0;
    uint64_t interval_max_swap_us = 0;
    unsigned long frames = 0;
    unsigned long interval_frames = 0;

    while (now_us() - start < (uint64_t)run_seconds * 1000000ull) {
        const float t = (float)((frames % 240ul) / 239.0f);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.05f + 0.15f * t, 0.10f, 0.25f + 0.25f * (1.0f - t), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const int box_w = width / 5;
        const int box_h = height / 5;
        const int travel_x = width > box_w ? width - box_w : 1;
        const int travel_y = height > box_h ? height - box_h : 1;
        const int x = (int)((frames * 7ul) % (unsigned long)travel_x);
        const int y = (int)((frames * 5ul) % (unsigned long)travel_y);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, box_w, box_h);
        glClearColor(1.0f - t, 0.2f + 0.7f * t, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        const uint64_t swap_start = now_us();
        if (!eglSwapBuffers(display, surface)) {
            printf("eglSwapBuffers failed error=0x%04x frame=%lu\n", eglGetError(), frames);
            break;
        }
        const uint64_t swap_us = now_us() - swap_start;
        total_swap_us += swap_us;
        interval_swap_us += swap_us;
        if (swap_us > max_swap_us) {
            max_swap_us = swap_us;
        }
        if (swap_us > interval_max_swap_us) {
            interval_max_swap_us = swap_us;
        }
        ++frames;
        ++interval_frames;

        const uint64_t now = now_us();
        if (now - interval_start >= 1000000ull) {
            const double seconds = (double)(now - interval_start) / 1000000.0;
            const double fps = (double)interval_frames / seconds;
            const double avg_swap_ms = interval_frames ? (double)interval_swap_us / (double)interval_frames / 1000.0 : 0.0;
            printf("swap_probe fps=%.2f frames=%lu avg_swap_ms=%.3f max_swap_ms=%.3f\n",
                   fps, interval_frames, avg_swap_ms, (double)interval_max_swap_us / 1000.0);
            fflush(stdout);
            interval_start = now;
            interval_frames = 0;
            interval_swap_us = 0;
            interval_max_swap_us = 0;
        }
    }

    const uint64_t done = now_us();
    const double total_seconds = (double)(done - start) / 1000000.0;
    const double fps = total_seconds > 0.0 ? (double)frames / total_seconds : 0.0;
    const double avg_swap_ms = frames ? (double)total_swap_us / (double)frames / 1000.0 : 0.0;
    printf("swap_probe_done seconds=%.2f frames=%lu fps=%.2f avg_swap_ms=%.3f max_swap_ms=%.3f\n",
           total_seconds, frames, fps, avg_swap_ms, (double)max_swap_us / 1000.0);

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    return 0;
}
