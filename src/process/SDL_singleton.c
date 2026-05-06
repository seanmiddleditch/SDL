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
static char *SingletonPath = NULL;
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

    SDL_SYS_QuitSingleton();

    SDL_free(SingletonPath);
    SingletonPath = NULL;

    IsSingleton = false;
}
