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
#include "modules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int procCount;
struct Proc *procs;

static int envCount = 0;
static char **envs = 0;

void blockbarSetEnv(const char *key, const char *val) {
    envs = realloc(envs, sizeof(char *) * ++envCount);
    envs[envCount - 1] = malloc(strlen(key) + 1);
    strcpy(envs[envCount - 1], key);

    setenv(key, val, 1);
}

static void resetEnvs() {
    if (envs == 0) {
        return;
    }

    for (int i = 0; i < envCount; i++) {
        setenv(envs[i], "", 1);
        free(envs[i]);
    }

    free(envs);

    envs = 0;
    envCount = 0;
}

static void barEnvs(struct Block *blk, int bar, struct Click *cd) {
    if (cd) {
        bar = cd->bar;
    }

    if (blk->eachmon || cd) {
        blockbarSetEnv("BAR_OUTPUT", bars[bar].output);
    }

    int rendered;

    if (blk->eachmon) {
        rendered = blk->data[bar].rendered;
    } else {
        rendered = blk->data->rendered;
    }

    if (!rendered) {
        return;
    }

    if (blk->eachmon || cd) {
        char x [12] = {0};
        sprintf(x, "%d", blk->x[bar] + bars[bar].x);
        blockbarSetEnv("BLOCK_X", x);
    }

    char w [12] = {0};
    sprintf(w, "%d", blk->width[bar]);
    blockbarSetEnv("BLOCK_WIDTH", w);
}

static void execute(struct Block *blk, int bar, struct Click *cd) {
    barEnvs(blk, bar, cd);

    int (*func)(struct Block *, int, struct Click *) =
        moduleGetFunction(blk->properties.module.val.STR, "exec");

    if (func) {
        int ret = func(blk, bar, cd);

        if (ret != 0) {
            goto end;
        }
    }

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
    proc->buffer = 0;

end:
    resetEnvs();
}

void blockExec(struct Block *blk, struct Click *cd) {
    if (!blk->properties.exec.val.STR ||
            strcmp(blk->properties.exec.val.STR, "") == 0) {
        return;
    }

    char button [12] = {0};
    char clickx [12] = {0};
    char blockid [12] = {0};

    if (cd != 0) {
        sprintf(button, "%d", cd->button);
        sprintf(clickx, "%d", cd->x + bars[cd->bar].x);
    }

    sprintf(blockid, "%d", blk->id);

    blockbarSetEnv("BLOCK_BUTTON", button);
    blockbarSetEnv("CLICK_X", clickx);
    blockbarSetEnv("BLOCK_ID", blockid);

    if (blk->eachmon) {
        if (cd) {
            execute(blk, cd->bar, cd);

            for (int i = 0; i < barCount; i++) {
                if (i == cd->bar) {
                    continue;
                }

                execute(blk, i, 0);
            }
        } else {
            for (int i = 0; i < barCount; i++) {
                execute(blk, i, 0);
            }
        }
    } else {
        execute(blk, 0, cd);
    }
}
