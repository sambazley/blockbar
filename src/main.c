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
#include "tray.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

static void printUsage(const char *file) {
    fprintf(stderr, "Usage: %s [config_file]\n", file);
}

static void blocksInit(struct Block *blocks, int count) {
    for (int i = 0; i < count; i++) {
        struct Block *blk = &blocks[i];
        if (blk->eachmon) {
            blk->data.mon = malloc(sizeof(*(blk->data.mon)) * barCount);
            memset(blk->data.mon, 0, sizeof(*(blk->data.mon)) * barCount);
        }
    }
}

static void blocksCleanup(struct Block *blocks, int count) {
    for (int i = 0; i < count; i++) {
        struct Block *blk = &blocks[i];
        if (blk->eachmon) {
            free(blk->data.mon);
        }
    }
}

static int gcd(int a, int b) {
    while (b) {
        a %= b;
        a ^= b;
        b ^= a;
        a ^= b;
    }

    return a;
}

static int getTickInterval() {
    int time = 0;

    for (int i = 0; i < leftBlockCount; i++) {
        time = gcd(time, leftBlocks[i].interval);
    }
    for (int i = 0; i < rightBlockCount; i++) {
        time = gcd(time, rightBlocks[i].interval);
    }

    for (int i = 0; i < leftBlockCount; i++) {
        leftBlocks[i].tickCount = leftBlocks[i].ticks =
            leftBlocks[i].interval / time;
    }
    for (int i = 0; i < rightBlockCount; i++) {
        rightBlocks[i].tickCount = rightBlocks[i].ticks =
            rightBlocks[i].interval / time;
    }

    return time;
}

static void tickBlock(struct Block *blk) {
    if (blk->tickCount == blk->ticks) {
        blk->tickCount = 0;
        blockExec(blk, 0);
    }

    blk->tickCount++;
}

static void tick() {
    for (int i = 0; i < leftBlockCount; i++) {
        tickBlock(&leftBlocks[i]);
    }
    for (int i = 0; i < rightBlockCount; i++) {
        tickBlock(&rightBlocks[i]);
    }
}

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

    configParse(config);

    renderInit();

    if (createBars() != 0) {
        return 1;
    }

    int trayBar = 0;

    if (conf.trayBar != 0) {
        for (int i = 0; i < barCount; i++) {
            struct Bar bar = bars[i];
            if (strcmp(bar.output, conf.trayBar) == 0) {
                trayBar = i;
                break;
            }
        }
    }

    trayInit(trayBar);

    blocksInit(leftBlocks, leftBlockCount);
    blocksInit(rightBlocks, rightBlockCount);

    redraw();

    struct timeval tv;
    fd_set fds;
    int x11fd = ConnectionNumber(disp);
    int interval = getTickInterval();

    while (1) {
        FD_ZERO(&fds);
        FD_SET(x11fd, &fds);

        tv.tv_sec = 0;
        tv.tv_usec = interval * 1000;

        int nfds = x11fd;
        for (int i = 0; i < procCount; i++) {
            struct Proc *proc = &procs[i];

            if (proc->pid) {
                FD_SET(proc->fdout, &fds);
                if (proc->fdout > nfds) {
                    nfds = proc->fdout;
                }
            }
        }

        int fdsRdy = select(nfds+1, &fds, 0, 0, &tv);

        pollEvents();

        if (fdsRdy == 0) {
            tick();
            continue;
        }

        for (int i = 0; i < procCount; i++) {
            struct Proc *proc = &procs[i];

            if (proc->pid == 0) continue;
            if (!FD_ISSET(proc->fdout, &fds)) continue;

            waitpid(proc->pid, 0, 0);

            char buf [2048] = {0};
            read(proc->fdout, buf, sizeof(buf) - 1);

            struct Block *blk = proc->blk;

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

            close(proc->fdout);

            proc->blk = 0;
            proc->pid = 0;
            proc->fdout = 0;

            break;
        }

        redraw();
    }

    trayCleanup();

    blocksCleanup(leftBlocks, leftBlockCount);
    blocksCleanup(rightBlocks, rightBlockCount);

    return 0;
}
