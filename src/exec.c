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
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int proc_count;
struct proc *procs;

static int env_count = 0;
static char **envs = 0;

void blockbar_set_env(const char *key, const char *val)
{
	envs = realloc(envs, sizeof(char *) * ++env_count);
	envs[env_count - 1] = malloc(strlen(key) + 1);
	strcpy(envs[env_count - 1], key);

	setenv(key, val, 1);
}

static void reset_envs()
{
	if (envs == 0) {
		return;
	}

	for (int i = 0; i < env_count; i++) {
		setenv(envs[i], "", 1);
		free(envs[i]);
	}

	free(envs);

	envs = 0;
	env_count = 0;
}

static void bar_envs(struct block *blk, int bar, struct click *cd)
{
	if (cd) {
		bar = cd->bar;
	}

	if (blk->eachmon || cd) {
		blockbar_set_env("BAR_OUTPUT", bars[bar].output);
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
		blockbar_set_env("BLOCK_X", x);

		char w [12] = {0};
		sprintf(w, "%d", blk->width[bar]);
		blockbar_set_env("BLOCK_WIDTH", w);
	}
}

static void execute(struct block *blk, int bar, struct click *cd)
{
	char blockid [12] = {0};
	sprintf(blockid, "%d", blk->id);
	blockbar_set_env("BLOCK_ID", blockid);

	bar_envs(blk, bar, cd);

	struct module *mod = get_module_by_name(blk->properties.module.val.STR);

	if (mod) {
		int (*func)(struct block *, int, struct click *) =
			module_get_function(mod, "exec");

		if (func) {
			int ret = func(blk, bar, cd);

			if (ret != 0) {
				goto end;
			}
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
		execl(shell, shell, "-c", blk->properties.exec.val.STR,
				(char *) 0);
	}

	close(out[1]);

	struct proc *proc = 0;
	for (int i = 0; i < proc_count; i++) {
		if (procs[i].pid == 0) {
			proc = &procs[i];
			break;
		}
	}

	if (proc == 0) {
		proc_count++;
		procs = realloc(procs, sizeof(struct proc) * proc_count);
		proc = &procs[proc_count - 1];
	}

	proc->fdout = out[0];
	proc->pid = pid;
	proc->blk = blk->id;
	proc->bar = bar;
	proc->buffer = 0;

end:
	reset_envs();
}

void block_exec(struct block *blk, struct click *cd)
{
	struct module *mod = get_module_by_name(blk->properties.module.val.STR);

	if (!mod) {
		return;
	}

	if (mod->data.flags & MFLAG_NO_EXEC) {
		return;
	}

	if (!blk->properties.exec.val.STR ||
			strcmp(blk->properties.exec.val.STR, "") == 0) {
		return;
	}

	char button [12] = {0};
	char clickx [12] = {0};

	if (cd != 0) {
		sprintf(button, "%d", cd->button);
		sprintf(clickx, "%d", cd->x + bars[cd->bar].x);
	}

	blockbar_set_env("BLOCK_BUTTON", button);
	blockbar_set_env("CLICK_X", clickx);

	if (blk->eachmon) {
		if (cd) {
			execute(blk, cd->bar, cd);

			for (int i = 0; i < bar_count; i++) {
				if (i == cd->bar) {
					continue;
				}

				execute(blk, i, 0);
			}
		} else {
			for (int i = 0; i < bar_count; i++) {
				execute(blk, i, 0);
			}
		}
	} else {
		execute(blk, 0, cd);
	}
}
