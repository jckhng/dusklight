#include <SDL3/SDL.h>
#include <SDL3/SDL_opengles2.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef void *RENDERDOC_DevicePointer;
typedef void *RENDERDOC_WindowHandle;
typedef void (*pRENDERDOC_GetAPIVersion)(int *major, int *minor, int *patch);
typedef void (*pRENDERDOC_SetCaptureFilePathTemplate)(const char *pathtemplate);
typedef void (*pRENDERDOC_TriggerCapture)(void);
typedef void (*pRENDERDOC_StartFrameCapture)(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle);
typedef unsigned int (*pRENDERDOC_EndFrameCapture)(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle);
typedef int (*pRENDERDOC_GetAPI)(int version, void **outAPIPointers);

struct RenderDocApi {
    pRENDERDOC_GetAPIVersion GetAPIVersion;
    void *SetCaptureOptionU32;
    void *SetCaptureOptionF32;
    void *GetCaptureOptionU32;
    void *GetCaptureOptionF32;
    void *SetFocusToggleKeys;
    void *SetCaptureKeys;
    void *GetOverlayBits;
    void *MaskOverlayBits;
    void *RemoveHooks;
    void *UnloadCrashHandler;
    pRENDERDOC_SetCaptureFilePathTemplate SetCaptureFilePathTemplate;
    void *GetCaptureFilePathTemplate;
    void *GetNumCaptures;
    void *GetCapture;
    pRENDERDOC_TriggerCapture TriggerCapture;
    void *IsTargetControlConnected;
    void *LaunchReplayUI;
    void *SetActiveWindow;
    pRENDERDOC_StartFrameCapture StartFrameCapture;
    void *IsFrameCapturing;
    pRENDERDOC_EndFrameCapture EndFrameCapture;
};

struct RenderDocState {
    int available;
    int capture_after;
    int capture_started;
    int capture_done;
    int use_start_end;
    struct RenderDocApi *api;
};

static const char *env_or_unset(const char *name) {
    const char *value = SDL_getenv(name);
    return value && value[0] ? value : "(unset)";
}

static int env_int(const char *name, int fallback) {
    const char *value = SDL_getenv(name);
    char *end = NULL;
    long parsed = 0;
    if (!value || !value[0]) {
        return fallback;
    }
    parsed = strtol(value, &end, 10);
    return end == value ? fallback : (int)parsed;
}

static void mkdir_parents(char *path) {
    size_t len = strlen(path);
    for (size_t i = 1; i < len; ++i) {
        if (path[i] == '/') {
            path[i] = '\0';
            mkdir(path, 0755);
            path[i] = '/';
        }
    }
    mkdir(path, 0755);
}

static void ensure_capture_dir(const char *path_template) {
    char buffer[1024];
    char *slash = NULL;
    if (!path_template || !path_template[0]) {
        return;
    }
    snprintf(buffer, sizeof(buffer), "%s", path_template);
    slash = strrchr(buffer, '/');
    if (!slash || slash == buffer) {
        return;
    }
    *slash = '\0';
    mkdir_parents(buffer);
}

static void print_video_drivers(void) {
    const int count = SDL_GetNumVideoDrivers();
    printf("SDL video driver count=%d\n", count);
    for (int i = 0; i < count; ++i) {
        printf("SDL video driver[%d]=%s\n", i, SDL_GetVideoDriver(i));
    }
}

static void pump_events(int *running, int ignore_input) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            *running = 0;
        }
        if (ignore_input) {
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            *running = 0;
        }
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN) {
            *running = 0;
        }
    }
}

static struct RenderDocState renderdoc_init(void) {
    struct RenderDocState state;
    const char *lib = SDL_getenv("PROBE_RENDERDOC_LIB");
    const char *path = SDL_getenv("PROBE_RENDERDOC_CAPTURE_PATH");
    const char *mode = SDL_getenv("PROBE_RENDERDOC_MODE");
    void *handle = NULL;
    pRENDERDOC_GetAPI get_api = NULL;
    void *api = NULL;
    int major = 0;
    int minor = 0;
    int patch = 0;

    memset(&state, 0, sizeof(state));
    state.capture_after = env_int("PROBE_RENDERDOC_CAPTURE_AFTER", 0);
    state.use_start_end = mode && strcmp(mode, "startend") == 0;
    if (state.capture_after <= 0) {
        printf("renderdoc disabled: PROBE_RENDERDOC_CAPTURE_AFTER=%d\n", state.capture_after);
        return state;
    }

    if (!lib || !lib[0]) {
        lib = "./renderdoc/lib/librenderdoc.so";
    }
    if (!path || !path[0]) {
        path = "./captures/sdl2shim-gles-present-probe";
    }
    ensure_capture_dir(path);

    handle = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        printf("renderdoc dlopen failed lib=%s error=%s\n", lib, dlerror());
        return state;
    }
    get_api = (pRENDERDOC_GetAPI)dlsym(handle, "RENDERDOC_GetAPI");
    if (!get_api) {
        printf("renderdoc RENDERDOC_GetAPI missing lib=%s\n", lib);
        return state;
    }
    if (!get_api(10102, &api) || !api) {
        printf("renderdoc API 1.1.2 unavailable\n");
        return state;
    }

    state.api = (struct RenderDocApi *)api;
    if (state.api->SetCaptureFilePathTemplate) {
        state.api->SetCaptureFilePathTemplate(path);
    }
    if (state.api->GetAPIVersion) {
        state.api->GetAPIVersion(&major, &minor, &patch);
    }
    state.available = state.api->TriggerCapture || (state.api->StartFrameCapture && state.api->EndFrameCapture);
    printf("renderdoc ready=%d api=%d.%d.%d lib=%s capture_after=%d mode=%s path=%s\n", state.available, major, minor,
           patch, lib, state.capture_after, state.use_start_end ? "startend" : "trigger", path);
    return state;
}

static void renderdoc_before_frame(struct RenderDocState *state, int frame) {
    if (!state->available || state->capture_done || state->capture_started || frame < state->capture_after) {
        return;
    }
    if (!state->use_start_end && state->api->TriggerCapture) {
        printf("renderdoc TriggerCapture frame=%d\n", frame);
        state->api->TriggerCapture();
        state->capture_done = 1;
        return;
    }
    if (state->api->StartFrameCapture && state->api->EndFrameCapture) {
        printf("renderdoc StartFrameCapture frame=%d\n", frame);
        state->api->StartFrameCapture(NULL, NULL);
        state->capture_started = 1;
    }
}

static void renderdoc_after_frame(struct RenderDocState *state, int frame) {
    unsigned int ok = 0;
    if (!state->capture_started || state->capture_done || !state->api || !state->api->EndFrameCapture) {
        return;
    }
    ok = state->api->EndFrameCapture(NULL, NULL);
    printf("renderdoc EndFrameCapture frame=%d result=%u\n", frame, ok);
    state->capture_started = 0;
    state->capture_done = 1;
}

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        printf("shader compile failed type=0x%04x log=%.*s\n", type, (int)len, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(void) {
    static const char *vs =
        "attribute vec2 a_pos;\n"
        "attribute vec4 a_color;\n"
        "attribute vec2 a_uv;\n"
        "varying vec4 v_color;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
        "  v_color = a_color;\n"
        "  v_uv = a_uv;\n"
        "}\n";
    static const char *fs =
        "precision mediump float;\n"
        "varying vec4 v_color;\n"
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_tex;\n"
        "uniform float u_use_tex;\n"
        "void main() {\n"
        "  vec4 tex = texture2D(u_tex, v_uv);\n"
        "  gl_FragColor = mix(v_color, v_color * tex, u_use_tex);\n"
        "}\n";
    GLuint program = 0;
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fs);
    GLint ok = 0;
    if (!vert || !frag) {
        return 0;
    }
    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glBindAttribLocation(program, 0, "a_pos");
    glBindAttribLocation(program, 1, "a_color");
    glBindAttribLocation(program, 2, "a_uv");
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog(program, sizeof(log), &len, log);
        printf("program link failed log=%.*s\n", (int)len, log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static GLuint create_texture(void) {
    const unsigned char pixels[] = {
        255, 255, 255, 255,  32, 180, 255, 255,
        255,  96,  32, 255,  32, 255, 128, 255,
    };
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return texture;
}

static void draw_scene(GLuint program, GLuint texture, int frame) {
    const float pulse = (float)(frame % 120) / 119.0f;
    const GLfloat triangle[] = {
        -0.75f, -0.55f, 1.0f, 0.1f, 0.1f, 1.0f, 0.0f, 0.0f,
         0.00f,  0.70f, 0.1f, 1.0f, 0.2f, 1.0f, 0.5f, 1.0f,
         0.75f, -0.55f, 0.2f, 0.2f, 1.0f, 1.0f, 1.0f, 0.0f,
    };
    const GLfloat quad[] = {
        -0.35f, -0.35f, 1.0f, pulse, 1.0f, 1.0f, 0.0f, 0.0f,
         0.85f, -0.35f, 1.0f, pulse, 1.0f, 1.0f, 1.0f, 0.0f,
        -0.35f,  0.45f, 1.0f, pulse, 1.0f, 1.0f, 0.0f, 1.0f,
        -0.35f,  0.45f, 1.0f, pulse, 1.0f, 1.0f, 0.0f, 1.0f,
         0.85f, -0.35f, 1.0f, pulse, 1.0f, 1.0f, 1.0f, 0.0f,
         0.85f,  0.45f, 1.0f, pulse, 1.0f, 1.0f, 1.0f, 1.0f,
    };
    const GLsizei stride = 8 * (GLsizei)sizeof(GLfloat);
    const GLint use_tex = glGetUniformLocation(program, "u_use_tex");

    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "u_tex"), 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glUniform1f(use_tex, 0.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, triangle);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, triangle + 2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, triangle + 6);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUniform1f(use_tex, 1.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, quad);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, quad + 2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, quad + 6);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

int main(int argc, char **argv) {
    int frames = 300;
    struct RenderDocState renderdoc;
    GLuint program = 0;
    GLuint texture = 0;
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
    printf("env PROBE_RENDERDOC_CAPTURE_AFTER=%s\n", env_or_unset("PROBE_RENDERDOC_CAPTURE_AFTER"));
    printf("env PROBE_RENDERDOC_MODE=%s\n", env_or_unset("PROBE_RENDERDOC_MODE"));
    printf("env PROBE_RENDERDOC_LIB=%s\n", env_or_unset("PROBE_RENDERDOC_LIB"));
    printf("env PROBE_RENDERDOC_CAPTURE_PATH=%s\n", env_or_unset("PROBE_RENDERDOC_CAPTURE_PATH"));
    printf("env PROBE_IGNORE_INPUT=%s\n", env_or_unset("PROBE_IGNORE_INPUT"));
    print_video_drivers();

    renderdoc = renderdoc_init();

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

    program = create_program();
    texture = create_texture();
    if (!program || !texture) {
        printf("GLES setup failed gl_error=0x%04x\n", glGetError());
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 6;
    }

    int running = 1;
    const int ignore_input = env_int("PROBE_IGNORE_INPUT", 0) != 0;
    for (int frame = 0; running && frame < frames; ++frame) {
        pump_events(&running, ignore_input);
        SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
        glViewport(0, 0, pixel_w, pixel_h);
        const float t = (float)(frame % 180) / 179.0f;
        const float r = frame < 60 ? 0.08f : (frame < 120 ? 0.0f : t * 0.25f);
        const float g = frame < 60 ? t * 0.25f : (frame < 120 ? 0.08f : 0.0f);
        const float b = frame < 60 ? 0.0f : (frame < 120 ? t * 0.25f : 0.08f);

        renderdoc_before_frame(&renderdoc, frame);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_scene(program, texture, frame);
        SDL_GL_SwapWindow(window);
        renderdoc_after_frame(&renderdoc, frame);

        if ((frame % 60) == 0) {
            const GLenum error = glGetError();
            printf("frame=%d pixels=%dx%d gl_error=0x%04x\n", frame, pixel_w, pixel_h, error);
            fflush(stdout);
        }
        SDL_Delay(16);
    }

    printf("present loop done\n");
    glDeleteTextures(1, &texture);
    glDeleteProgram(program);
    SDL_GL_MakeCurrent(window, NULL);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("sdl2shim-gles-present-probe done\n");
    return 0;
}
