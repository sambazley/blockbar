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

#include "blocks.h"
#include "config.h"
#include "exec.h"
#include "render.h"
#include "socket.h"
#include "tray.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int interval;

static void printUsage(const char *file) {
    fprintf(stderr, "Usage: %s [config_file]\n", file);
}

static void blocksCleanup() {
    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        if (blk->id) {
            removeBlock(blk);
        }
    }
}

static void tickBlock(struct Block *blk, int interval) {
    if (blk->interval == 0) return;

    if (blk->timePassed >= blk->interval) {
        blk->timePassed = 0;
        blockExec(blk, 0);
    }

    blk->timePassed += interval;
}

static void getTime(struct timeval *tv) {
    clock_gettime(CLOCK_MONOTONIC_RAW, (struct timespec *) tv);
    tv->tv_usec /= 1000;
}

#define TIMEDIFF(a, b) (long) (((b.tv_sec - a.tv_sec) * 1000000) \
                              + (b.tv_usec - a.tv_usec))

int main(int argc, const char *argv[]) {
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

    JsonObject *jsonConfig = configInit(config);

    if (jsonConfig) {
        configParseGeneral(jsonConfig);
    }

    renderInit();

    if (createBars() != 0) {
        return 1;
    }

    if (jsonConfig) {
        configParseBlocks(jsonConfig);
        configCleanup(jsonConfig);
    }

    int trayBar = 0;

    if (settings.traybar.val.STR != 0) {
        for (int i = 0; i < barCount; i++) {
            struct Bar bar = bars[i];
            if (strcmp(bar.output, settings.traybar.val.STR) == 0) {
                trayBar = i;
                break;
            }
        }
    }

    trayInit(trayBar);

    int sockfd = socketInit();

    for (int i = 0; i < blockCount; i++) {
        blockExec(&blocks[i], 0);
    }

    redraw();

    struct timeval tv, timer1, timer2;
    fd_set fds;
    int x11fd = ConnectionNumber(disp);

    updateTickInterval();

    getTime(&timer1);

    while (1) {
        FD_ZERO(&fds);
        if (sockfd > 0) {
            FD_SET(sockfd, &fds);
        }
        FD_SET(x11fd, &fds);

        getTime(&timer2);
        long elapsed = TIMEDIFF(timer1, timer2);

        tv.tv_sec = 0;
        tv.tv_usec = MAX(interval * 1000 - elapsed, 0);

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

        int fdsRdy = select(nfds+1, &fds, 0, 0, interval == 0 ? 0 : &tv);

        pollEvents();

        if (fdsRdy == 0) {
            getTime(&timer1);
            for (int i = 0; i < blockCount; i++) {
                tickBlock(&blocks[i], interval);
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

            waitpid(proc->pid, 0, 0);

            char buf [2048] = {0};
            read(proc->fdout, buf, sizeof(buf) - 1);

            struct Block *blk = getBlock(proc->blk);

            if (blk) {
                char **execData;
                if (blk->eachmon) {
                    execData = &(blk->data.mon[proc->bar].type.legacy.execData);
                } else {
                    execData = &(blk->data.type.legacy.execData);
                }

                if (*execData) {
                    free(*execData);
                }
                *execData = malloc(strlen(buf) + 1);
                strcpy(*execData, buf);

                if (strlen(buf) && (*execData)[strlen(buf) - 1] == '\n') {
                    (*execData)[strlen(buf) - 1] = 0;
                }
            }

            close(proc->fdout);

            proc->blk = 0;
            proc->pid = 0;
            proc->fdout = 0;

            break;
        }

        redraw();
    }

    trayCleanup();

    blocksCleanup();

    return 0;
}
