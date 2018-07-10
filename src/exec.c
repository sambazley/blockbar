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
#include <stdio.h>
#include <stdlib.h>
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
        execl(shell, shell, "-c", blk->exec, (char *) 0);
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
    proc->blk = blk;
    proc->bar = bar;
}

void blockExec(struct Block *blk, struct Click *cd) {
    if (!blk->exec) return;

    char button [2] = {0};
    char subblock [4] = {0};
    if (cd != 0) {
        button[0] = cd->button + '0';
        sprintf(subblock, "%u", cd->subblock);
    }
    setenv("BLOCK_BUTTON", button, 1);
    setenv("SUBBLOCK", subblock, 1);

    if (blk->eachmon) {
        if (cd) {
            setenv("BAR_OUTPUT", bars[cd->bar].output, 1);
            execute(blk, cd->bar);

            setenv("BLOCK_BUTTON", "", 1);
            for (int i = 0; i < barCount; i++) {
                if (i == cd->bar) continue;

                setenv("BAR_OUTPUT", bars[i].output, 1);
                execute(blk, i);
            }
        } else {
            for (int i = 0; i < barCount; i++) {
                setenv("BAR_OUTPUT", bars[i].output, 1);
                execute(blk, i);
            }
        }
    } else {
        setenv("BAR_OUTPUT", "", 1);
        execute(blk, 0);
    }
}
