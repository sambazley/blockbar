/* Copyright (C) 2018 Sam Bazley
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "config.h"
#include "exec.h"
#include "modules.h"
#include "render.h"
#include "socket.h"
#include "task.h"
#include "tray.h"
#include "window.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef CLOCK_MONOTONIC_RAW
#   define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

int interval;

static void printUsage(const char *file) {
    fprintf(stderr, "Usage: %s [config_file]\n", file);
}

static void cleanupBlocks() {
    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        if (blk->id) {
            removeBlock(blk);
        }
    }
}

static void onexit() {
    static int exited = 0;

    if (exited) {
        return;
    }

    exited = 1;

    cleanupTray();
    cleanupBlocks();
    cleanupModules();
    cleanupBars();
    cleanupSettings();
    cleanupTasks();

    if (blocks) {
        free(blocks);
    }

    if (procs) {
        free(procs);
    }

    exit(0);
}

int main(int argc, const char *argv[]) {
    signal(SIGTERM, onexit);
    signal(SIGINT, onexit);
    atexit(onexit);

    const char *config = "";
    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printUsage(argv[0]);
            return 1;
        }

        config = argv[1];
    } else if (argc > 2) {
        printUsage(argv[0]);
        return 1;
    }

    if (createBars() != 0) {
        return 1;
    }

    JsonObject *jsonConfig = configInit(config);

    if (jsonConfig) {
        configParseGeneral(jsonConfig);
    }

    updateGeom();

    if (!isSettingModified(&settings.traybar)) {
        trayInit(0);
    }

    initModules();

    if (jsonConfig) {
        configParseBlocks(jsonConfig);
        configCleanup(jsonConfig);
    }

    int sockfd = socketInit();

    for (int i = 0; i < blockCount; i++) {
        blockExec(&blocks[i], 0);
    }

    redraw();

    struct timeval tv;
    fd_set fds;
    int x11fd = ConnectionNumber(disp);

    while (1) {
        FD_ZERO(&fds);
        if (sockfd > 0) {
            FD_SET(sockfd, &fds);
        }
        FD_SET(x11fd, &fds);

        tv = getTimeToNextTask();

        int nfds = MAX(x11fd, sockfd);
        for (int i = 0; i < procCount; i++) {
            struct Proc *proc = &procs[i];

            if (proc->pid) {
                FD_SET(proc->fdout, &fds);
                if (proc->fdout > nfds) {
                    nfds = proc->fdout;
                }
            }
        }

        int fdsRdy = select(nfds+1, &fds, 0, 0, tv.tv_sec < 0 || tv.tv_usec < 0 ? 0 : &tv);

        if (fdsRdy == -1) {
            continue;
        }

        pollEvents();

        if (fdsRdy == 0) {
            tickTasks();

            if (moduleRedrawDirty) {
                moduleRedrawDirty = 0;
                redraw();
            }

            continue;
        }

        if (FD_ISSET(sockfd, &fds)) {
            socketRecv(sockfd);
            continue;
        }

        for (int i = 0; i < procCount; i++) {
            struct Proc *proc = &procs[i];

            if (proc->pid == 0) continue;
            if (!FD_ISSET(proc->fdout, &fds)) continue;

            char buf [2048] = {0};
            int r = read(proc->fdout, buf, sizeof(buf) - 1);

            if (r < 0) {
                fprintf(stderr, "Error reading fdout\n");
                continue;
            }

            if (!proc->buffer) {
                proc->buffer = malloc(strlen(buf) + 1);
                strcpy(proc->buffer, buf);
            } else {
                proc->buffer = realloc(proc->buffer,
                                       strlen(proc->buffer) + strlen(buf) + 1);
                strcpy(proc->buffer + strlen(proc->buffer), buf);
            }

            if (waitpid(proc->pid, 0, WNOHANG) == 0) continue;

            struct Block *blk = getBlock(proc->blk);

            if (blk) {
                char **execData;
                if (blk->eachmon) {
                    execData = &(blk->data[proc->bar].execData);
                } else {
                    execData = &(blk->data->execData);
                }

                if (*execData) {
                    free(*execData);
                }
                *execData = proc->buffer;

                if (strlen(*execData) &&
                        (*execData)[strlen(*execData) - 1] == '\n') {
                    (*execData)[strlen(*execData) - 1] = 0;
                }

                redrawBlock(blk);
            }

            close(proc->fdout);

            proc->blk = 0;
            proc->pid = 0;
            proc->fdout = 0;

            break;
        }

        redraw();
    }

    return 0;
}
