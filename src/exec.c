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

#include "exec.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int procCount;
struct Proc *procs;

static void execute(struct Block *blk, int bar) {
    int out [2];

    if (pipe(out) == -1) {
        fprintf(stderr, "Failed to create pipe\n");
        return;
    }

    int pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Failed to fork\n");
        return;
    }

    if (pid == 0) {
        close(out[0]);

        dup2(out[1], STDOUT_FILENO);

        close(out[1]);

        char *shell = "/bin/sh";
        execl(shell, shell, "-c", blk->properties.exec.val.STR, (char *) 0);
    }

    close(out[1]);

    struct Proc *proc = 0;
    for (int i = 0; i < procCount; i++) {
        if (procs[i].pid == 0) {
            proc = &procs[i];
            break;
        }
    }

    if (proc == 0) {
        procCount++;
        procs = realloc(procs, sizeof(struct Proc) * procCount);
        proc = &procs[procCount - 1];
    }

    proc->fdout = out[0];
    proc->pid = pid;
    proc->blk = blk->id;
    proc->bar = bar;
}

static void barEnvs(struct Block *blk, int bar, struct Click *cd) {
    if (cd) {
        bar = cd->bar;
    }

    if (blk->eachmon || cd) {
        setenv("BAR_OUTPUT", bars[bar].output, 1);
    } else {
        setenv("BAR_OUTPUT", "", 1);
    }

    setenv("SUBBLOCK_WIDTHS", "", 1);

    int rendered;

    if (blk->eachmon) {
        rendered = blk->data.mon[bar].type.legacy.rendered;
    } else {
        rendered = blk->data.type.legacy.rendered;
    }

    if (!rendered) {
        setenv("BLOCK_X", "", 1);
        setenv("BLOCK_WIDTH", "", 1);
        return;
    }

    char x [12] = {0};
    sprintf(x, "%d", blk->x[bar] + bars[bar].x);
    setenv("BLOCK_X", x, 1);

    char w [12] = {0};
    sprintf(w, "%d", blk->width[bar]);
    setenv("BLOCK_WIDTH", w, 1);

    if (blk->properties.mode.val.MODE == SUBBLOCK) {
        int sbc;
        int *widths;
        if (blk->eachmon) {
            sbc = blk->data.mon[bar].type.subblock.subblockCount;
            widths = blk->data.mon[bar].type.subblock.widths;
        } else {
            sbc = blk->data.type.subblock.subblockCount;
            widths = blk->data.type.subblock.widths;
        }

        char *sbw = 0;

        for (int i = 0; i < sbc; i++) {
            char str [13] = {0};
            sprintf(str, "%d\n", widths[i]);

            if (sbw == 0) {
                sbw = malloc(strlen(str) + 1);
                *sbw = 0;
            } else {
                sbw = realloc(sbw, strlen(sbw) + strlen(str) + 1);
            }

            strcat(sbw, str);
        }

        if (sbw) {
            sbw[strlen(sbw) - 1] = 0;

            setenv("SUBBLOCK_WIDTHS", sbw, 1);

            free(sbw);
        }
    }
}

void blockExec(struct Block *blk, struct Click *cd) {
    if (!blk->properties.exec.val.STR ||
            strcmp(blk->properties.exec.val.STR, "") == 0) {
        return;
    }

    char button [12] = {0};
    char subblock [12] = {0};
    char clickx [12] = {0};

    if (cd != 0) {
        sprintf(button, "%d", cd->button);
        sprintf(subblock, "%d", cd->subblock);
        sprintf(clickx, "%d", cd->x + bars[cd->bar].x);
    }

    setenv("BLOCK_BUTTON", button, 1);
    setenv("SUBBLOCK", subblock, 1);
    setenv("CLICK_X", clickx, 1);

    if (blk->eachmon) {
        if (cd) {
            barEnvs(blk, cd->bar, cd);
            execute(blk, cd->bar);

            setenv("BLOCK_BUTTON", "", 1);
            setenv("SUBBLOCK", "", 1);
            setenv("CLICK_X", "", 1);

            for (int i = 0; i < barCount; i++) {
                if (i == cd->bar) {
                    continue;
                }

                barEnvs(blk, i, 0);
                execute(blk, i);
            }
        } else {
            for (int i = 0; i < barCount; i++) {
                barEnvs(blk, i, 0);
                execute(blk, i);
            }
        }
    } else {
        barEnvs(blk, 0, cd);
        execute(blk, 0);
    }
}
