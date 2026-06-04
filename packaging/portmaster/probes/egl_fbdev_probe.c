#define LINUX 1
#define EGL_API_FB 1

#include <EGL/egl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct fbdev_window {
    unsigned short width;
    unsigned short height;
};

static void print_count(EGLDisplay display, const char *label, const EGLint *attribs) {
    EGLint count = 0;
    EGLBoolean ok = eglChooseConfig(display, attribs, NULL, 0, &count);
    printf("%s ok=%d count=%d error=0x%04x\n", label, ok, count, eglGetError());
}

int main(void) {
    int fd = open("/dev/fb0", O_RDWR, 0);
    struct fb_var_screeninfo vinfo;
    if (fd < 0) {
        perror("open /dev/fb0");
        return 1;
    }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("FBIOGET_VSCREENINFO");
        close(fd);
        return 2;
    }
    close(fd);

    struct fbdev_window window = {
        .width = (unsigned short)vinfo.xres,
        .height = (unsigned short)vinfo.yres,
    };
    printf("fb=%ux%u bpp=%u\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    printf("display=%p error=0x%04x\n", (void *)display, eglGetError());
    if (display == EGL_NO_DISPLAY) {
        return 3;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(display, &major, &minor)) {
        printf("eglInitialize failed error=0x%04x\n", eglGetError());
        return 4;
    }
    printf("egl=%d.%d vendor=%s\n", major, minor, eglQueryString(display, EGL_VENDOR));

    const EGLint any_attribs[] = { EGL_NONE };
    const EGLint window_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };
    const EGLint gles2_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    const EGLint rgb_gles2_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    const EGLint rgb_window_gles2_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    const EGLint rgb565_window_gles2_attribs[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_BLUE_SIZE, 5,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    print_count(display, "any", any_attribs);
    print_count(display, "window", window_attribs);
    print_count(display, "gles2", gles2_attribs);
    print_count(display, "rgb_gles2", rgb_gles2_attribs);
    print_count(display, "rgb_window_gles2", rgb_window_gles2_attribs);
    print_count(display, "rgb565_window_gles2", rgb565_window_gles2_attribs);

    EGLConfig config = 0;
    EGLint count = 0;
    if (!eglChooseConfig(display, rgb565_window_gles2_attribs, &config, 1, &count) || count < 1) {
        printf("chosen config failed error=0x%04x count=%d\n", eglGetError(), count);
        eglTerminate(display);
        return 5;
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)&window, NULL);
    printf("surface=%p error=0x%04x\n", (void *)surface, eglGetError());
    if (surface != EGL_NO_SURFACE) {
        EGLBoolean bind_ok = eglBindAPI(EGL_OPENGL_ES_API);
        printf("bind_gles=%d error=0x%04x\n", bind_ok, eglGetError());

        const EGLint context_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
        printf("context=%p error=0x%04x\n", (void *)context, eglGetError());
        if (context != EGL_NO_CONTEXT) {
            EGLBoolean current_ok = eglMakeCurrent(display, surface, surface, context);
            printf("make_current=%d error=0x%04x\n", current_ok, eglGetError());
            eglDestroyContext(display, context);
        }
    }
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
    }
    eglTerminate(display);
    return surface == EGL_NO_SURFACE ? 6 : 0;
}
