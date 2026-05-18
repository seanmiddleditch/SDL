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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

struct SingletonPosixClient
{
    char *buffer;
    int sock;
    Uint16 length;
    Uint16 capacity;
    bool connected;
};

static char *LockFilePath = NULL;
static char *ListenSocketPath = NULL;

static int LockFileFd = -1;
static int ListenSocketFd = -1;

static struct SingletonPosixClient clients[4];

bool SDL_SYS_InitSingleton(const char *root, bool *out_is_singleton)
{
    if (SDL_asprintf(&LockFilePath, "%s/singleton.pid", root) < 0) {
        return SDL_SetError("Failed to allocate pid path");
    }

    if (SDL_asprintf(&ListenSocketPath, "%s/singleton.sock", root) < 0) {
        return SDL_SetError("Failed to allocate socket path");
    }

    LockFileFd = open(LockFilePath, O_CREAT | O_RDWR, 0666);
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

    SDL_RemovePath(LockFilePath);

    SDL_free(LockFilePath);
    LockFilePath = NULL;

    SDL_free(ListenSocketPath);
    ListenSocketPath = NULL;
}

static bool SetNonBlocking(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        return SDL_SetError("fcntl F_GETFL failed: %s", strerror(errno));
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        return SDL_SetError("fcntl F_SETFL failed: %s", strerror(errno));
    }
    return true;
}

bool SDL_SYS_SingletonBeginListen(const char *root)
{
    ListenSocketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ListenSocketFd < 0) {
        return SDL_SetError("Failed to create singleton socket: %s", strerror(errno));
    }

    if (!SetNonBlocking(ListenSocketFd)) {
        goto fail;
    }

    if (!SDL_RemovePath(ListenSocketPath)) {
        return false;
    }

    struct sockaddr_un addr;
    SDL_zero(addr);

    addr.sun_family = AF_UNIX;
    SDL_strlcpy(addr.sun_path, ListenSocketPath, sizeof addr.sun_path);

    if (bind(ListenSocketFd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
        SDL_SetError("Failed to bind singleton socket: %s", strerror(errno));
        goto fail;
    }

    if (listen(ListenSocketFd, 3) < 0) {
        SDL_SetError("Failed to listen on singleton socket: %s", strerror(errno));
        goto fail;
    }

    return true;

fail:
    close(ListenSocketFd);
    ListenSocketFd = -1;
    return false;
}

void SDL_SYS_SingletonEndListen(void)
{
    for (int i = 0; i < SDL_arraysize(clients); ++i) {
        if (clients[i].connected) {
            close(clients[i].sock);
            clients[i].sock = -1;
            clients[i].connected = false;

            SDL_free(clients[i].buffer);
            clients[i].buffer = NULL;
            clients[i].length = 0;
            clients[i].capacity = 0;
        }
    }

    if (ListenSocketFd >= 0) {
        close(ListenSocketFd);
        ListenSocketFd = -1;
    }
}

bool SDL_SYS_SendSingletonMessage(const char *root, const char *message, size_t size, Sint32 timeoutMS)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return SDL_SetError("Failed to create singleton socket: %s", strerror(errno));
    }

    if (timeoutMS >= 0) {
        if (!SetNonBlocking(sock)) {
            goto fail;
        }
    }

    struct sockaddr_un addr;
    SDL_zero(addr);

    addr.sun_family = AF_UNIX;
    SDL_strlcpy(addr.sun_path, ListenSocketPath, sizeof addr.sun_path);

    bool connected = false;

    const char *const end = message + size;

    for (;;) {
        if (timeoutMS > 0) {
            struct pollfd pfd;
            SDL_zero(pfd);
            pfd.events = POLLOUT;
            pfd.fd = sock;

            SDL_Time start;
            SDL_GetCurrentTime(&start);

            int rs = poll(&pfd, 1, timeoutMS);
            if (rs < 0 && errno != EINTR) {
                SDL_SetError("Failed to poll singleton socket: %s", strerror(errno));
                goto fail;
            }
            if (rs == 0) {
                SDL_SetError("Failed to %s to singleton socket: timeout", connected ? "write" : "connect");
                goto fail;
            }

            SDL_Time now;
            SDL_GetCurrentTime(&now);

            Uint64 elapsedMS = SDL_NS_TO_MS(now - start);
            timeoutMS -= elapsedMS;
            if (timeoutMS < 0) {
                timeoutMS = 0;
            }
        }

        if (!connected) {
            int rs = connect(sock, &addr, sizeof addr);
            if (rs < 0 && (errno != EAGAIN || timeoutMS == 0)) {
                SDL_SetError("Failed to connect on singleton socket: %s", strerror(errno));
                goto fail;
            }
            if (rs == 0) {
                connected = true;
            }
        }

        if (connected) {
            int rs = write(sock, message, end - message);
            if (rs < 0 && (errno != EAGAIN || timeoutMS == 0) && errno != EINTR) {
                SDL_SetError("Failed to write to singleton socket: %s", strerror(errno));
                goto fail;
            }
            if (rs > 0) {
                message += rs;
                if (message == end) {
                    break;
                }
            }
        }
    }

    close(sock);
    return true;

fail:
    close(sock);
    return false;
}

void SDL_SYS_UpdateListen(void)
{
    int avail = -1;

    /* FIXME: one poll syscall would be better than one each... if there's more than one client,
     * which probably isn't a serious concern */
    for (int i = 0; i < SDL_arraysize(clients); ++i) {
        if (!clients[i].connected) {
            avail = i;
            continue;
        }

        if (!clients[i].buffer) {
            clients[i].buffer = SDL_malloc(SDL_SYS_SingletonMessageBufferInitialSize);
            if (!clients[i].buffer) {
                SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to allocate singleton read buffer");
                goto close;
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
                goto close;
            }

            clients[i].buffer = new_buffer;
            clients[i].capacity = new_capacity;
        }

        int rs = read(clients[i].sock, clients[i].buffer + clients[i].length, clients[i].capacity - clients[i].length);
        if (rs < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Singleton socket read failed: %s", strerror(errno));
        }

        if (rs > 0) {
            clients[i].length += rs;
        }

        if (rs == 0) {
            if (!SDL_SYS_PushSingletonMessageEvent(clients[i].buffer, clients[i].length)) {
                SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Singleton message could not be dispatched: %s", SDL_GetError());
            }
            goto close;
        }

        continue;

close:
        close(clients[i].sock);
        clients[i].connected = false;
        avail = i;
    }

    if (avail >= 0) {
        clients[avail].sock = accept(ListenSocketFd, NULL, NULL);
        if (clients[avail].sock >= 0) {
            if (!SetNonBlocking(clients[avail].sock)) {
                close(clients[avail].sock);
                SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Singleton socket SetNonBlocking failed: %s", SDL_GetError());
                return;
            }
            clients[avail].connected = true;
            clients[avail].length = 0;
        }
    }
}
