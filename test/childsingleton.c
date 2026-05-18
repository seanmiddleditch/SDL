/**
 * Singleton test suite (child)
 */
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_singleton.h>
#include <SDL3/SDL_test.h>

static SDLTest_CommonState *state = NULL;

static const char *options[] = {
    "NAME",
    NULL
};

static int PrimarySingleton()
{
    SDL_Init(SDL_INIT_EVENTS);

    SDL_Window *window = SDL_CreateWindow("child singleton", 0, 0, SDL_WINDOW_HIDDEN);

    SDLTest_Log("[child] Starting as primary instance");

    if (!SDL_IsSingleton()) {
        SDLTest_LogError("[child] Not primary singleton");
        SDL_DestroyWindow(window);
        return 2;
    }

    SDL_Time deadline;
    SDL_GetCurrentTime(&deadline);
    deadline += SDL_SECONDS_TO_NS(1000);

    SDL_Event ev;
    for (;;) {
        if (!SDL_PollEvent(&ev)) {
            SDL_Delay(100);
            continue;
        }

        if (ev.type == SDL_EVENT_SINGLETON_MESSAGE) {
            SDLTest_Log("[child] Received singleton event");
            SDL_DestroyWindow(window);
            return 0;
        }

        SDL_Time now;
        SDL_GetCurrentTime(&now);
        if (now >= deadline) {
            SDLTest_Log("[child] SDL_WaitEvent timeout");
            SDL_DestroyWindow(window);
            return 3;
        }
    }


}

int main(int argc, char *argv[])
{
    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    const char *param = NULL;

    /* Parse commandline */
    for (int i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!param) {
                param = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }
        i += consumed;
    }

    if (param == NULL) {
        SDLTest_CommonLogUsage(state, argv[0], options);
        return 1;
    }

    if (!SDL_InitSingleton("libsdl", "test_singleton")) {
        SDLTest_LogError("[child] SDL_InitSingleton failed: %s", SDL_GetError());
        return 1;
    }

    if (SDL_strcmp(param, "__primary__") == 0) {
        return PrimarySingleton();
    }

    if (SDL_IsSingleton()) {
        SDLTest_LogError("[child] Unexpectedly primary singleton");
        return 2;
    }

    const char *params[] = {
        param,
        "childsingleton",
        NULL,
    };
    if (!SDL_NotifySingleton(params, 1500)) {
        SDLTest_LogError("[child] SDL_NotifySingleton failed: %s", SDL_GetError());
        return 3;
    }

    SDL_QuitSingleton();
    return 0;
}