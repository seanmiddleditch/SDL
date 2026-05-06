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
#include "../../io/SDL_iostream_c.h"
#include "../SDL_syssingleton.h"
#include "io/SDL_sysasyncio.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

static char *LockFilePath = NULL;

static int LockFileFd = -1;

bool SDL_SYS_InitSingleton(const char *root, bool *out_is_singleton)
{
    if (SDL_asprintf(&LockFilePath, "%s/singleton.pid", root) < 0) {
        return SDL_SetError("Failed to allocate pid path");
    }

    LockFileFd = open(LockFilePath, O_CREAT|O_RDWR, 0666);
    if (LockFileFd < 0) {
        SDL_free(LockFilePath);
        LockFilePath = NULL;
        return SDL_SetError("Failed to open lock file: %s", strerror(errno));
    }

    struct flock fl = {
        .l_len = 1,
        .l_start = 0,
        .l_pid = getpid(),
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
    };

    if (fcntl(LockFileFd, F_SETLK, &fl) < 0) {
        close(LockFileFd);
        LockFileFd = -1;
        SDL_free(LockFilePath);
        LockFilePath = NULL;
        *out_is_singleton = false;
        return true;
    }

    *out_is_singleton = true;
    dprintf(LockFileFd, "%d\n", getpid());
    return true;
}

void SDL_SYS_QuitSingleton(void)
{
    if (LockFileFd >= 0) {
        close(LockFileFd);
        SDL_RemovePath(LockFilePath);
        LockFileFd = -1;
    }

    SDL_free(LockFilePath);
    LockFilePath = NULL;
}
