#define LINUX 1
#define EGL_API_FB 1
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct fbdev_window {
    unsigned short width;
    unsigned short height;
};

static void try_context(EGLDisplay display, EGLConfig config, EGLSurface surface, int major, int minor) {
    const EGLint attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, major,
        EGL_NONE,
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, attribs);
    printf("context_es%d.%d=%p error=0x%04x\n", major, minor, (void *)context, eglGetError());
    if (context == EGL_NO_CONTEXT) {
        return;
    }
    EGLBoolean current_ok = eglMakeCurrent(display, surface, surface, context);
    printf("make_current_es%d.%d=%d error=0x%04x\n", major, minor, current_ok, eglGetError());
    if (current_ok) {
        printf("gl_version_es%d.%d=%s\n", major, minor, glGetString(GL_VERSION));
        printf("gl_vendor_es%d.%d=%s\n", major, minor, glGetString(GL_VENDOR));
        printf("gl_renderer_es%d.%d=%s\n", major, minor, glGetString(GL_RENDERER));
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
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

    struct fbdev_window window = { (unsigned short)vinfo.xres, (unsigned short)vinfo.yres };
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint egl_major = 0;
    EGLint egl_minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &egl_major, &egl_minor)) {
        printf("egl init failed display=%p error=0x%04x\n", (void *)display, eglGetError());
        return 2;
    }
    printf("egl=%d.%d vendor=%s client_apis=%s\n", egl_major, egl_minor, eglQueryString(display, EGL_VENDOR),
           eglQueryString(display, EGL_CLIENT_APIS));

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
    eglBindAPI(EGL_OPENGL_ES_API);
    try_context(display, config, surface, 2, 0);
    try_context(display, config, surface, 3, 0);
    try_context(display, config, surface, 3, 1);
    try_context(display, config, surface, 3, 2);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    return 0;
}
