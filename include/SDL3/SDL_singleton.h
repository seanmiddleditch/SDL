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

/**
 * # CategorySingleton
 *
 * Unique application instance.
 *
 * This API lets apps protect against multiple instances of the app running
 * simultaneously, and provides very rudimentary IPC (Inter-Process Communication)
 * to the singleton instance from any secondary instances.
 *
 * A primary use case is to allow tool application with a unified or single
 * editor model to prevent multiple instances from opening, and to allow
 * parameters to secondary instances to be forwarded to the singleton instance.
 *
 * Platforms that do not support the necessary mechanisms will report every
 * instance as being the singleton instance. Platforms without the
 * necessary IPC mechanisms will fail to send any message to the singleton
 * instance.
 */

#ifndef SDL_singleton_h_
#define SDL_singleton_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes and performs the singleton instance test.
 *
 * If the process is determined to be the singleton instance, the
 * process will automatically register to be the recipient of
 * SDL_NotifySingleton calls from other processes.
 *
 * If detection fails for any reason, false is returned and SDL_GetError
 * contains the reason for failure. A return value of true does not mean
 * that this process is the singleton instance; test that with
 * SDL_IsSingleton() instead.
 *
 * The org and app name are the same values that should be passed to
 * SDL_GetPrefPath, and should follow the same rules and guidelines.
 *
 * \param org the name of your organization.
 * \param app the name of your application.
 * \returns true if singleton detection initialized correctly, false otherwise.
 *          Check SDL_GetError for cause of failure.
 *
 * \threadsafety This function should only be called on the main thread.
 *
 * \since This function is available since SDL 3.x.x.
 *
 * \sa SDL_IsSingleton
 * \sa SDL_QuitSingleton
 * \sa SDL_GetPrefPath
 */
extern SDL_DECLSPEC bool SDLCALL SDL_InitSingleton(const char *org, const char *app);

/**
 * Tests if the current process is the singleton instance.
 *
 * After calling SDL_InitSingleton, this returns true if the current
 * process is the singleton instance. It also returns true if there
 * was an error initializing the singleton detection, or if
 * the platform does not support this feature.
 *
 * If this function returns false, then for sure this is _not_ the
 * singleton instance at the time SDL_InitSingleton was
 * invoked. It is always safe to exit the process if this function
 * to prevent multiple instances of the process.
 *
 * \returns false If there was already another singleton instance
 *          at the time of SDL_InitSingleton, and true in any
 *          other case.
 *
 * \since This function is available since SDL 3.x.x.
 *
 * \sa SDL_QuitSingleton
 */
extern SDL_DECLSPEC bool SDLCALL SDL_IsSingleton();

/**
 * Release ownership of the singleton identifier.
 *
 * If the current process is the singleton instance, it will be released.
 * The next call by any process to SDL_InitSingleton will take ownership
 * of the singleton.
 *
 * If the current process is not the singleton, this function has no effect.
 *
 * \threadsafety This function should only be called on the main thread.
 *
 * \since This function is available since SDL 3.x.x.
 *
 * \sa SDL_InitSingleton
 */
extern SDL_DECLSPEC void SDLCALL SDL_QuitSingleton(void);

/**
 * Sends a message to the current singleton instance.
 *
 * If this process is the singleton, it will receive the message. If another
 * instance is the singleton, the message will be sent via IPC.
 *
 * SDL_InitSingleton _must_ have been invoked prior to this function.
 *
 * This function will block until the message is sent or sending fails.
 * FIXME: allow specifying a timeout value?
 *
 * The params array is one or more strings which will be sent to the
 * owner of the instance, and the params list should be terminated
 * with a NULL, e.g.:
 *
 * ```c
 * const char *params[] = { "first", "second", NULL };
 * ```
 *
 * \param params the arguments for the message.
 * \param timeoutMS timeout in microseconds, or -1 to wait indefinitely.
 *
 * \threadsafety This function should only be called on the main thread.
 *
 * \since This function is available since SDL 3.x.x.
 *
 * \sa SDL_InitSingleton
 */
extern SDL_DECLSPEC bool SDLCALL SDL_NotifySingleton(const char * const *params, Sint32 timeoutMS);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_singleton_h_ */
