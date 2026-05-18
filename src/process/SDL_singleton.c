/*
Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../events/SDL_events_c.h"
#include "SDL_internal.h"
#include "SDL_syssingleton.h"

static bool IsSingleton = false;
static bool IsListening = false;
static char *SingletonPath = NULL;

bool SDLCALL SDL_SYS_PushSingletonMessageEvent(const char *buffer, size_t size)
{
    Sint32 arg_count = 0;
    for (const char *ch = buffer; ch != buffer + size; ++ch) {
        if (*ch == '\0') {
            ++arg_count;
        }
    }

    char *text = SDL_AllocateTemporaryMemory(size + 1 /* safety NULL byte */);
    if (!text) {
        return SDL_SetError("Failed to allocate message buffer");
    }

    SDL_memcpy(text, buffer, size);
    text[size] = '\0';

    const char **args = SDL_AllocateTemporaryMemory((sizeof(const char *) * (arg_count + 1)));
    if (!args) {
        return SDL_SetError("Failed to allocate message buffer");
    }

    Sint32 arg_index = 0;
    args[0] = text;
    for (const char *ch = text; ch != text + size && arg_index < arg_count; ++ch) {
        if (*ch == '\0') {
            ++arg_index;
            args[arg_index] = ch + 1;
        }
    }
    args[arg_count] = NULL;

    SDL_Event event;
    SDL_zero(event);
    event.singleton.type = SDL_EVENT_SINGLETON_MESSAGE;
    event.singleton.timestamp = SDL_GetTicksNS();
    event.singleton.args = args;
    event.singleton.num_args = arg_count;
    return SDL_PushEvent(&event);
}

bool SDL_InitSingleton(const char *org, const char *app)
{
    if (!org) {
        return SDL_InvalidParamError("org");
    }

    if (!app) {
        return SDL_InvalidParamError("app");
    }

    if (SingletonPath) {
        return SDL_SetError("Redundant call to SDL_InitSingleton");
    }

    SingletonPath = SDL_GetPrefPath(org, app);
    if (!SingletonPath) {
        return SDL_SetError("SDL_GetPrefPath returned NULL");
    }

    if (!SDL_SYS_InitSingleton(SingletonPath, &IsSingleton)) {
        SDL_free(SingletonPath);
        SingletonPath = NULL;
        return false;
    }

    if (IsSingleton) {
        IsListening = SDL_SYS_SingletonBeginListen(SingletonPath);
        if (!IsListening) {
            SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Singleton listening failing: %s", SDL_GetError());
        }
    }

    return true;
}

bool SDL_IsSingleton(void)
{
    /* If initialization failed for any reason, we assume we are
     * the singleton instance */
    if (!SingletonPath) {
        return true;
    }

    return IsSingleton;
}

void SDL_QuitSingleton(void)
{
    if (!SingletonPath) {
        SDL_SetError("Invalid call to SDL_QuitSingleton without prior call to SDL_InitSingleton");
        return;
    }

    if (IsListening) {
        SDL_SYS_SingletonEndListen();
        IsListening = false;
    }

    SDL_SYS_QuitSingleton();

    SDL_free(SingletonPath);
    SingletonPath = NULL;

    IsSingleton = false;
}

bool SDL_NotifySingleton(const char *const *params, Sint32 timeoutMS)
{
    char small[1024];

    if (!params) {
        return SDL_InvalidParamError("params");
    }

    if (!SingletonPath) {
        return SDL_SetError("Invalid call to SDL_NotifySingleton without prior call to SDL_InitSingleton");
    }

    /* FIXME: handle overflow? */
    Uint32 size = 0;
    for (int i = 0; params[i] != NULL; ++i) {
        size += SDL_strlen(params[i]);
        size += 1; /* NULL byte */
    }

    if (size > SDL_SYS_SingletonMessageBufferMaxSize) {
        return SDL_SetError("Message too long");
    }


    char *buffer = small;
    if (size > sizeof small) {
        buffer = SDL_malloc(size);
        if (!buffer) {
            return SDL_SetError("Failed to allocate message buffer");
        }
    }

    char *out = buffer;
    for (int i = 0; params[i] != NULL; ++i) {
        size_t length = SDL_strlen(params[i]);
        /* Copy param including trailing NULL byte */
        SDL_memcpy(out, params[i], length + 1);
        out += length + 1;
    }

    bool result = SDL_SYS_SendSingletonMessage(SingletonPath, buffer, size, timeoutMS);

    if (buffer != small) {
        SDL_free(buffer);
    }

    return result;
}

void SDL_UpdateSingleton(void)
{
    if (IsListening) {
        SDL_SYS_UpdateListen();
    }
}