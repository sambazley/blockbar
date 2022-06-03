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
#ifndef WAYLAND
#include "tray.h"
#endif
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

#define MAX(a, b) (a>b?a:b)

int interval;

static void print_usage(const char *file)
{
	fprintf(stderr, "Usage: %s [config_file]\n", file);
}

static void cleanup_blocks()
{
	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];
		if (blk->id) {
			remove_block(blk);
		}
	}
}

static void onexit()
{
	static int exited = 0;

	if (exited) {
		return;
	}

	exited = 1;

#ifndef WAYLAND
	cleanup_tray();
#endif
	cleanup_blocks();
	cleanup_modules();
	cleanup_bars();
	cleanup_settings();
	cleanup_tasks();

	if (blocks) {
		free(blocks);
	}

	if (procs) {
		free(procs);
	}

	exit(0);
}

int main(int argc, const char *argv[])
{
	signal(SIGTERM, onexit);
	signal(SIGINT, onexit);
	atexit(onexit);

	const char *config = "";
	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
			print_usage(argv[0]);
			return 1;
		}

		config = argv[1];
	} else if (argc > 2) {
		print_usage(argv[0]);
		return 1;
	}

	if (create_bars() != 0) {
		return 1;
	}

	JsonObject *json_config = config_init(config);

	if (json_config) {
		config_parse_general(json_config);
	}

	update_geom();

#ifndef WAYLAND
	if (!is_setting_modified(&settings.traybar)) {
		tray_init(0);
	}
#endif

	modules_init();

	if (json_config) {
		config_parse_blocks(json_config);
		config_cleanup(json_config);
	}

	int sockfd = socket_init();

	for (int i = 0; i < block_count; i++) {
		block_exec(&blocks[i], 0);
	}

	redraw();

	struct timeval tv;
	fd_set fds;
#ifdef WAYLAND
	int dispfd = wl_display_get_fd(disp);
#else
	int dispfd = ConnectionNumber(disp);
#endif

	while (1) {
		FD_ZERO(&fds);
		if (sockfd > 0) {
			FD_SET(sockfd, &fds);
		}
		FD_SET(dispfd, &fds);

		tv = get_time_to_next_task();

		int nfds = MAX(dispfd, sockfd);
		for (int i = 0; i < proc_count; i++) {
			struct proc *proc = &procs[i];

			if (proc->pid) {
				FD_SET(proc->fdout, &fds);
				if (proc->fdout > nfds) {
					nfds = proc->fdout;
				}
			}
		}

		int fds_rdy = select(nfds + 1, &fds, 0, 0,
				tv.tv_sec < 0 || tv.tv_usec < 0 ? 0 : &tv);

		if (fds_rdy == -1) {
			continue;
		}

		poll_events();

		if (fds_rdy == 0) {
			tick_tasks();

			if (module_redraw_dirty) {
				module_redraw_dirty = 0;
				redraw();
			}

			continue;
		}

		if (FD_ISSET(sockfd, &fds)) {
			socket_recv(sockfd);
			continue;
		}

		for (int i = 0; i < proc_count; i++) {
			struct proc *proc = &procs[i];

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

			struct block *blk = get_block(proc->blk);

			if (blk) {
				char **exec_data;
				if (blk->eachmon) {
					exec_data = &(blk->data[proc->bar].exec_data);
				} else {
					exec_data = &(blk->data->exec_data);
				}

				if (*exec_data) {
					free(*exec_data);
				}
				*exec_data = proc->buffer;

				if (strlen(*exec_data) &&
						(*exec_data)[strlen(*exec_data) - 1] == '\n') {
					(*exec_data)[strlen(*exec_data) - 1] = 0;
				}

				redraw_block(blk);
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
