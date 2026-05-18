/**
 * Singleton test suite
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL_main.h>
#include <SDL3/SDL_singleton.h>
#include <SDL3/SDL_test.h>

static const int TimeoutSeconds = 10;

typedef struct
{
    SDL_Process *process;
    char name[32];
    int state; /* 0 pending, 1 success, -1 error */
    bool started;
} ChildStateInfo;

static SDLTest_CommonState *state = NULL;
static ChildStateInfo children[12] = {};

#ifdef SDL_PLATFORM_WINDOWS
#define EXE ".exe"
#else
#define EXE ""
#endif

static Uint64 SDL_DECLSPEC TimerCalled(void *userdata, SDL_TimerID timer, Uint64 interval)
{
    /* our signal that something went amiss */
    SDL_Event event;
    event.type = SDL_EVENT_USER;
    event.common.timestamp = SDL_GetTicksNS();
    SDL_PushEvent(&event);
    return 0;
}

static const char *options[] = {
    "/path/to/childsingleton" EXE,
    NULL
};

static const char *child_path = NULL;

static bool HandleMessage(Sint32 count, const char *const *params)
{
    if (count != 2) {
        SDLTest_LogError("Received incorrect number of arguments; got %d expected 2", count);
        return false;
    }

    if (SDL_strcmp(params[1], "childsingleton") != 0) {
        SDLTest_LogError("Incorrect first argument; got '%s' expected '%s'", params[1], "childsingleton");
        return false;
    }

    if (params[2] != NULL) {
        SDLTest_LogError("Missing NULL sentinel in array");
        return false;
    }

    /* mark child complete */
    for (int i = 0; i < SDL_arraysize(children); ++i) {
        if (children[i].state == 0 && SDL_strcmp(params[0], children[i].name) == 0) {
            SDLTest_Log("Received message from child %d", i);
            children[i].state = 1;
            return true;
        }
    }

    SDLTest_LogError("Incorrect second argument; got '%s' matches no expected string", params[1]);
    return false;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    /* Parse commandline */
    for (int i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!child_path) {
                child_path = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!child_path) {
        SDLTest_CommonLogUsage(state, argv[0], options);
        return 1;
    }

    static const char alphabet[] = "abcdefghijkmnopqrstuvwxyz0123456789"; // alphanumeric, except l (ell) because it looks like 1 (one) on some fonts
    static const Uint32 alphabet_size = SDL_arraysize(alphabet) - 1;

    SDL_zero(children);

    for (int i = 0; i < SDL_arraysize(children); ++i) {
        SDL_memset(children[i].name, 0, SDL_arraysize(children[i].name));
        for (int j = 0; j < SDL_arraysize(children[i].name) - 1; ++j) {
            children[i].name[j] = alphabet[SDL_rand(alphabet_size)];
        }
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (!SDL_InitSingleton("libsdl", "test_singleton")) {
        SDLTest_LogError("SDL_InitSingleton failed: %s", SDL_GetError());
        return 1;
    }

    if (!SDL_IsSingleton()) {
        SDLTest_LogError("Not singleton");
        return SDL_APP_FAILURE;
    }

    if (SDL_AddTimerNS(SDL_SECONDS_TO_NS(TimeoutSeconds), TimerCalled, NULL) == 0) {
        SDLTest_LogError("Failed to create timer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_USER:
        SDLTest_LogError("Test timed out");
        return SDL_APP_FAILURE;
    case SDL_EVENT_SINGLETON_MESSAGE:
        if (!HandleMessage(event->singleton.num_args, event->singleton.args)) {
            return SDL_APP_FAILURE;
        }
        return SDL_APP_CONTINUE;
    default:
        break;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

static SDL_AppResult TestNotify()
{
    SDL_QuitSingleton();

    /* Test that sending a message works; do this in _this_ process to ease
     * debugging if the test fails, because auto-attaching to the children
     * is somewhat of a pain depending on environment */
    const char *args[] = {
        child_path,
        "__primary__",
        NULL,
    };
    SDL_Process *primary = SDL_CreateProcess(args, false);
    if (!primary) {
        SDLTest_LogError("SDL_CreateProcess failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Give process time to start */
    SDL_Delay(500);

    if (!SDL_InitSingleton("libsdl", "test_singleton")) {
        SDLTest_LogError("SDL_InitSingleton failed: %s", SDL_GetError());
        goto fail;
    }

    if (SDL_IsSingleton()) {
        SDLTest_LogError("Unexpectedly singleton");
        goto fail;
    }

    const char *params[] = { "ping", NULL };
    if (!SDL_NotifySingleton(params, 500)) {
        SDLTest_LogError("SDL_NotifySingleton failed: %s", SDL_GetError());
        goto fail;
    }

    int exit;
    SDL_WaitProcess(primary, true, &exit);
    if (exit != 0) {
        SDLTest_LogError("Primary instance failed with exit code: %d", exit);
        return SDL_APP_FAILURE;
    }
    return SDL_APP_SUCCESS;

fail:
    SDL_KillProcess(primary, true);
    SDL_WaitProcess(primary, true, NULL);
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    int exit;
    int allstate = 1;
    int remaining = 0;

    for (int i = 0; i < SDL_arraysize(children); ++i) {
        if (children[i].process) {
            if (SDL_WaitProcess(children[i].process, false, &exit)) {
                SDL_DestroyProcess(children[i].process);
                children[i].process = NULL;

                if (exit != 0) {
                    children[i].state = -1;
                    SDLTest_LogError("Child process %d returned error: %d", i, exit);
                }
            } else {
                ++remaining;
            }
        } else if (!children[i].started) {
            children[i].started = true;

            const char *args[] = {
                child_path,
                children[i].name,
                NULL,
            };

            children[i].process = SDL_CreateProcess(args, false);
            if (!children[i].process) {
                SDLTest_LogError("SDL_CreateProcess failed: %s", SDL_GetError());
                children[i].state = -1;
            }

            SDLTest_Log("Spawned child %d", i);

            ++remaining;
        }

        if (children[i].state < allstate) {
            allstate = children[i].state;
        }
    }

    if (remaining == 0) {
        if (allstate > 0) {
            return TestNotify();
        }
        if (allstate < 0) {
            TestNotify();
            return SDL_APP_FAILURE;
        }
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* Clean up any child if it hasn't terminated yet */
    for (int i = 0; i < SDL_arraysize(children); ++i) {
        if (children[i].process) {
            if (!SDL_WaitProcess(children[i].process, false, NULL)) {
                /* FIXME: small race here where the process can die between the wait and the kill, but there's no timeout
                 *        and no way to check _why_ kill process failed; this can only happen in the timeout failure
                 *        case anyway, so just allow the "harmless" superfluous error? */
                if (!SDL_KillProcess(children[i].process, true)) {
                    SDLTest_LogError("SDL_KillProcess failed: %s", SDL_GetError());
                }
            }
            SDL_DestroyProcess(children[i].process);
            children[i].process = NULL;
        }
    }

    SDL_QuitSingleton();

    SDLTest_CommonQuit(state);
}
