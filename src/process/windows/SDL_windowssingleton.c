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
#include "../../core/windows/SDL_windows.h"
#include "../SDL_syssingleton.h"
#include "SDL_internal.h"

static HANDLE Mutex = NULL;

bool SDL_SYS_InitSingleton(const char *root, bool *out_is_singleton)
{
    char mutex_name[PATH_MAX];

    Uint32 hash = SDL_murmur3_32(root, SDL_strlen(root), 0);
    SDL_snprintf(mutex_name, sizeof mutex_name, "Local\\SDL-%08x", hash);

    /* Try to take ownership of the instance. Note that the pipe name is
     * used to send messages by non-owners, so it must be set first. */
    Mutex = CreateMutexA(NULL, true, mutex_name);
    if (!Mutex) {
        return WIN_SetError("Failed to create shared mutex");
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(Mutex);
        Mutex = NULL;
        *out_is_singleton = false;
        return true;
    }

    *out_is_singleton = true;
    return true;
}

void SDL_SYS_QuitSingleton(void)
{
    if (Mutex != NULL) {
        CloseHandle(Mutex);
        Mutex = NULL;
    }
}
