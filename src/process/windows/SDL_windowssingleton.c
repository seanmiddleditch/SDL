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
#include "SDL3/SDL_test_log.h"
#include "SDL_internal.h"

static HANDLE Mutex = NULL;

enum SingletonWindowClientState
{
    STATE_RESET,
    STATE_CONNECTING,
    STATE_READING,
};

struct SingletonWindowsClient
{
    HANDLE event;
    HANDLE pipe;
    enum SingletonWindowClientState state;
    bool pending;
    OVERLAPPED overlapped;
    char *buffer;
    Uint16 length;
    Uint16 capacity;
};

static struct SingletonWindowsClient clients[3];

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

bool SDL_SYS_SingletonBeginListen(const char *root)
{
    char pipe_name[PATH_MAX];

    Uint32 hash = SDL_murmur3_32(root, SDL_strlen(root), 0);
    SDL_snprintf(pipe_name, PATH_MAX, "\\\\.\\pipe\\SDL-%08x", hash);

    for (int i = 0; i < SDL_arraysize(clients); ++i) {
        SDL_zero(clients[i]);
        clients[i].pipe = INVALID_HANDLE_VALUE;
    }

    bool ok = false;

    for (int i = 0; i < SDL_arraysize(clients); ++i) {
        SDL_zero(clients[i]);

        DWORD flags = PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED;
        if (i == 0) {
            flags |= FILE_FLAG_FIRST_PIPE_INSTANCE;
        }

        clients[i].event = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (clients[i].event == NULL) {
            WIN_SetError("CreateEventA");
            SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "%s", SDL_GetError());
            continue;
        }

        clients[i].pipe = CreateNamedPipeA(pipe_name, flags, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, SDL_arraysize(clients), 512, 512, 0, NULL);
        if (clients[i].pipe == INVALID_HANDLE_VALUE) {
            WIN_SetError("CreateNamedPipeA");
            SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "%s", SDL_GetError());
            continue;
        }

        clients[i].overlapped.hEvent = clients[i].event;

        ok = true;
    }

    if (!ok) {
        SDL_SYS_SingletonEndListen();
        return false;
    }

    return true;
}

void SDL_SYS_SingletonEndListen(void)
{
    for (int i = 0; i < SDL_arraysize(clients); ++i) {
        if (clients[i].event) {
            CloseHandle(clients[i].event);
            clients[i].event = NULL;
        }

        if (clients[i].pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(clients[i].pipe);
            clients[i].pipe = INVALID_HANDLE_VALUE;
        }
    }
}

void SDL_SYS_UpdateListen(void)
{
    for (int i = 0; i < SDL_arraysize(clients); ++i) {
        if (clients[i].pipe == INVALID_HANDLE_VALUE) {
            continue;
        }

        DWORD read = 0;
        bool ok = false;

        if (clients[i].state == STATE_RESET) {
            ok = ConnectNamedPipe(clients[i].pipe, &clients[i].overlapped);
            if (ok) {
                clients[i].state = STATE_READING;
                clients[i].pending = false;
            } else if (GetLastError() == ERROR_IO_PENDING) {
                clients[i].state = STATE_CONNECTING;
                clients[i].pending = true;
            } else if (GetLastError() == ERROR_PIPE_CONNECTED) {
                SetEvent(clients[i].event);
                clients[i].state = STATE_READING;
                clients[i].pending = false;
                clients[i].length = 0;
            } else {
                WIN_SetError("ConnectNamedPipe");
                CloseHandle(clients[i].pipe);
                clients[i].pipe = INVALID_HANDLE_VALUE;
                continue;
            }
        }

        /* Check the IO result and handle it based on _current_ state, and set the
         * next state which will be used for the response */
        if (clients[i].pending) {
            DWORD signaled = WaitForSingleObjectEx(clients[i].event, 0, TRUE);
            if (signaled == WAIT_FAILED) {
                WIN_SetError("WaitForMultipleObjects");
                CloseHandle(clients[i].pipe);
                clients[i].pipe = INVALID_HANDLE_VALUE;
                continue;
            }
            if (signaled == WAIT_TIMEOUT) {
                continue;
            }

            ResetEvent(clients[i].event);

            ok = GetOverlappedResult(clients[i].event, &clients[i].overlapped, &read, FALSE);

            switch (clients[i].state) {
            case STATE_RESET:
                SDL_assert(false && "Invalid state");
                break;
            case STATE_CONNECTING:
                if (!ok) {
                    CloseHandle(clients[i].pipe);
                    clients[i].pipe = INVALID_HANDLE_VALUE;
                }

                clients[i].state = STATE_READING;
                clients[i].length = 0;
                break;
            case STATE_READING:
                if (!ok || read == 0) {
                    DisconnectNamedPipe(clients[i].pipe);
                    clients[i].state = STATE_RESET;
                    continue;
                }

                SDL_SYS_PushSingletonMessageEvent(clients[i].buffer, read);
                DisconnectNamedPipe(clients[i].pipe);
                clients[i].state = STATE_RESET;
                break;
            }
        }

        /* Process next step based on (maybe new) state */
        switch (clients[i].state) {
        case STATE_RESET:
        case STATE_CONNECTING:
            break;
        case STATE_READING:
            if (!clients[i].buffer) {
                clients[i].buffer = SDL_malloc(SDL_SYS_SingletonMessageBufferInitialSize);
                if (!clients[i].buffer) {
                    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to allocate singleton read buffer");
                    DisconnectNamedPipe(clients[i].pipe);
                    clients[i].state = STATE_RESET;
                    break;
                }
                clients[i].capacity = SDL_SYS_SingletonMessageBufferInitialSize;
            }

            if (clients[i].length == clients[i].capacity) {
                int new_capacity = clients[i].capacity * 2;
                if (new_capacity > SDL_SYS_SingletonMessageBufferMaxSize) {
                    new_capacity = SDL_SYS_SingletonMessageBufferMaxSize;
                }

                char *new_buffer = SDL_realloc(clients[i].buffer, new_capacity);
                if (!new_buffer) {
                    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to grow singleton read buffer");
                    DisconnectNamedPipe(clients[i].pipe);
                    clients[i].state = STATE_RESET;
                    break;
                }

                clients[i].buffer = new_buffer;
                clients[i].capacity = new_capacity;
            }

            clients[i].overlapped.Pointer = clients[i].buffer + clients[i].length;
            ok = ReadFile(clients[i].pipe, clients[i].buffer, clients[i].capacity - clients[i].length, &read, &clients[i].overlapped);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                clients[i].pending = true;
                continue;
            }

            if (ok) {
                if (read > 0) {
                    clients[i].length += read;
                    continue;
                }
                SDL_SYS_PushSingletonMessageEvent(clients[i].buffer, clients[i].length);
            }

            DisconnectNamedPipe(clients[i].pipe);
            clients[i].state = STATE_RESET;
            break;
        }
    }
}

bool SDL_SYS_SendSingletonMessage(const char *root, const char *message, size_t size, Sint32 timeoutMS)
{
    char pipe_name[PATH_MAX];

    Uint32 hash = SDL_murmur3_32(root, SDL_strlen(root), 0);
    SDL_snprintf(pipe_name, PATH_MAX, "\\\\.\\pipe\\SDL-%08x", hash);

    HANDLE pipe;
    for (;;) {
        pipe = CreateFileA(pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipe != INVALID_HANDLE_VALUE) {
            break;
        }

        if (GetLastError() != ERROR_PIPE_BUSY) {
            return WIN_SetError("CreateFileA");
        }

        if (timeoutMS == 0) {
            return SDL_SetError("Timed out");
        }

        if (timeoutMS < 0) {
            WaitNamedPipeA(pipe_name, NMPWAIT_WAIT_FOREVER);
            continue;
        }

        SDL_Time start;
        SDL_GetCurrentTime(&start);

        if (!WaitNamedPipeA(pipe_name, timeoutMS)) {
            return SDL_SetError("Timed out");
        }

        SDL_Time now;
        SDL_GetCurrentTime(&now);

        Uint64 elapsedMS = SDL_NS_TO_MS(now - start);
        timeoutMS -= elapsedMS;
        if (timeoutMS < 0) {
            timeoutMS = 0;
        }
    }

    DWORD written;
    if (!WriteFile(pipe, message, size, &written, NULL)) {
        WIN_SetError("WriteFile");
        CloseHandle(pipe);
        return false;
    }

    if (written < size) {
        SDL_SetError("Failed to write complete message");
        CloseHandle(pipe);
        return false;
    }

    CloseHandle(pipe);
    return true;
}
