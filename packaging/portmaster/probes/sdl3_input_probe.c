#include <SDL3/SDL.h>
#include <stdio.h>

static const char* button_name(SDL_GamepadButton button) {
    const char* name = SDL_GetGamepadStringForButton(button);
    return name != NULL ? name : "unknown";
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const char* mapping_file = SDL_getenv("PROBE_GAMECONTROLLERDB");
    if (mapping_file != NULL && mapping_file[0] != '\0') {
        int added = SDL_AddGamepadMappingsFromFile(mapping_file);
        printf("mapping_file=%s added=%d err=%s\n", mapping_file, added, SDL_GetError());
    }

    if (!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    printf("joystick_count=%d\n", count);
    for (int i = 0; i < count; ++i) {
        SDL_JoystickID id = ids[i];
        printf("device[%d] id=%d name=%s gamepad=%d\n",
               i,
               (int)id,
               SDL_GetJoystickNameForID(id),
               SDL_IsGamepad(id) ? 1 : 0);
        if (SDL_IsGamepad(id)) {
            SDL_Gamepad* gamepad = SDL_OpenGamepad(id);
            printf("  open_gamepad=%p name=%s mapping=%s\n",
                   (void*)gamepad,
                   gamepad != NULL ? SDL_GetGamepadName(gamepad) : "(null)",
                   gamepad != NULL ? SDL_GetGamepadMapping(gamepad) : "(null)");
        } else {
            SDL_Joystick* joy = SDL_OpenJoystick(id);
            printf("  open_joystick=%p\n", (void*)joy);
        }
    }
    SDL_free(ids);

    printf("Press buttons; Ctrl+C to exit.\n");
    fflush(stdout);

    SDL_Event event;
    while (1) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
                printf("JOY_BUTTON_DOWN which=%d button=b%d\n",
                       (int)event.jbutton.which,
                       (int)event.jbutton.button);
                fflush(stdout);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                SDL_GamepadButton button = (SDL_GamepadButton)event.gbutton.button;
                printf("GAMEPAD_BUTTON_DOWN which=%d button=%s(%d)\n",
                       (int)event.gbutton.which,
                       button_name(button),
                       (int)button);
                fflush(stdout);
                break;
            }
            case SDL_EVENT_GAMEPAD_ADDED:
                printf("GAMEPAD_ADDED which=%d\n", (int)event.gdevice.which);
                fflush(stdout);
                break;
            case SDL_EVENT_JOYSTICK_ADDED:
                printf("JOY_ADDED which=%d\n", (int)event.jdevice.which);
                fflush(stdout);
                break;
            }
        }
        SDL_Delay(5);
    }

    return 0;
}
