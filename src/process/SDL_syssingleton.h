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
#include "SDL_internal.h"

#ifndef SDL_syssingleton_
#define SDL_syssingleton_

/* Yes this is a Yes/No/Maybe */
typedef enum SDL_SYS_SingletonStatus
{
    SDL_SINGLETON_UNKNOWN = 0,
    SDL_SINGLETON_TRUE,
    SDL_SINGLETON_FALSE,
} SDL_SYS_SingletonStatus;

bool SDL_SYS_InitSingleton(const char *root, bool *out_is_singleton);
void SDL_SYS_QuitSingleton(void);

#endif // SDL_syssingleton_
