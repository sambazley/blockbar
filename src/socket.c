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

#include "socket.h"
#include "bbc.h"
#include "config.h"
#include "exec.h"
#include "modules.h"
#include "render.h"
#include "types.h"
#ifndef WAYLAND
#include "tray.h"
#endif
#include "util.h"
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int socket_init()
{
	signal(SIGPIPE, SIG_IGN);

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Error opening socket\n");
		return -1;
	}

	char *socketpath = getenv("BLOCKBAR_SOCKET");
	if (!socketpath) {
		socketpath = defsocketpath;
	}

	struct sockaddr_un sock_addr;

	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, socketpath);

	unlink(socketpath);
	if (bind(sockfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) == -1) {
		fprintf(stderr, "Error binding socket\n");
		return -1;
	}

	if (listen(sockfd, SOMAXCONN) == -1) {
		fprintf(stderr, "Error listening to socket\n");
		return -1;
	}

	return sockfd;
}

#define cmd(x) \
	static int cmd_##x(int argc, char **argv, int fd)

#define frprintf(f, fmt, ...) \
	dprintf(fd, "%c%c", setout, f); \
	dprintf(fd, fmt, ##__VA_ARGS__);

#define rprintf(fmt, ...) \
	frprintf(rstdout, fmt, ##__VA_ARGS__)

#define vars(n, usage, eachmon) \
	if (argc != (n)) { \
		rprintf("Usage: %s %s <block index>%s " usage "\n", \
				argv[0], argv[1], (eachmon ? "[:output]" : "")); \
		return 1; \
	} \
	if (strcmp(argv[2], "") == 0) { \
		rprintf("Index is null\n"); \
		return 1; \
	} \
	char *end; \
	int index = strtol(argv[2], &end, 0); \
	int output = -1; \
	if (eachmon && *end == ':' && end != argv[2] + strlen(argv[2]) - 1) { \
		output = -2; \
		char *output_name = end + 1; \
		if (strcmp(output_name, "*") == 0) { \
			output = -1; \
		} \
		for (int i = 0; i < bar_count && output == -2; i++) { \
			struct bar *bar = &bars[i]; \
			if (strcmp(output_name, bar->output) == 0) { \
				output = i; \
				break; \
			} \
		} \
		if (output == -2) { \
			rprintf("Output does not exist\n"); \
			return 1; \
		} \
	} else if (*end != 0) { \
		rprintf("Invalid index, expecting integer\n"); \
		return 1; \
	} \
	struct block *blk = get_block(index); \
	if (!blk) { \
		rprintf("No block at index %d\n", index); \
		return 1; \
	}

cmd(help)
{
	(void) argc;

#define phelp(key, val) \
	rprintf("\t%-27s%s\n", key, val)

	rprintf("Usage: %s <command>\n\n", argv[0]);
	rprintf("Commands:\n");
	phelp("list", "Lists blocks by their indices and \"exec\" value");
	phelp("exec <n>", "Executes block's script");
	phelp("list-properties", "Lists a block's properties");
	phelp("list-settings", "Lists the bar's settings");
	phelp("property <n>[:o] <p> [v]", "Gets or sets a property of a block");
	phelp("setting [m:]<s> [v]", "Gets or sets a setting of the bar");
	phelp("new [--eachmon]", "Creates a new block");
	phelp("rm <n>", "Removes a block");
	phelp("move-left <n>", "Moves a block left");
	phelp("move-right <n>", "Moves a block right");
	phelp("dump [--explicit]", "Dumps the current configuration to stdout");
	phelp("list-modules", "Lists the loaded modules");
	phelp("load-module <file>", "Loads a module");
	phelp("unload-module <name>", "Unloads a module");
	phelp("raise <name>", "Raises a render module");
	phelp("lower <name>", "Lowers a render module");

#undef phelp

	return 0;
}

cmd(list)
{
	(void) argc;
	(void) argv;

	int max_id_columns = 0;
	int max_id = 0;

	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];

		if (blk->id > max_id) {
			max_id = blk->id;
		}
	}

	while (max_id) {
		max_id_columns++;
		max_id /= 10;
	}

	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];
		if (blk->id) {
			rprintf("%-*u" "%c %s\n",
					max_id_columns + 1, blk->id,
					blk->eachmon ? '*' : ' ',
					blocks[i].properties.exec.val.STR);
		}
	}

	return 0;
}

cmd(exec)
{
	vars(3, "", 0);

	block_exec(blk, 0);
	return 0;
}

cmd(list_properties)
{
	(void) argc;
	(void) argv;

#define p(t, v, d) \
	rprintf("%-9s%-17s%s\n", t, v, d);

	for (int i = 0; i < property_count; i++) {
		struct setting *property = &((struct setting *) &def_properties)[i];
		p(type_strings[property->type], property->name, property->desc);
	}

	p("string", "execdata", "Data that is displayed");
#undef p

	return 0;
}

cmd(list_settings)
{
	(void) argc;
	(void) argv;

	for (int i = 0; i < setting_count; i++) {
		struct setting *setting = &((struct setting *) &settings)[i];
		rprintf("%-9s%-15s%s\n", type_strings[setting->type],
				setting->name, setting->desc);
	}

	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];

		if (!mod->dl) {
			continue;
		}

		if (mod->data.setting_count == 0) {
			continue;
		}

		rprintf("\n%s:\n", mod->data.name);

		for (int j = 0; j < mod->data.setting_count; j++) {
			struct setting *setting = &mod->data.settings[j];
			rprintf("%-9s%-15s%s\n", type_strings[setting->type],
					setting->name, setting->desc);
		}
	}

	return 0;
}

static void print_setting(struct setting *setting, int fd)
{
	switch (setting->type) {
	case INT:
		rprintf("%d\n", setting->val.INT);
		break;
	case BOOL:
		rprintf("%s\n", setting->val.BOOL ? "true" : "false");
		break;
	case STR:
		rprintf("%s\n", setting->val.STR);
		break;
	case COL:
		rprintf("#%02x%02x%02x%02x\n",
				setting->val.COL[0],
				setting->val.COL[1],
				setting->val.COL[2],
				setting->val.COL[3]);
		break;
	case POS:
		if (setting->val.POS == LEFT) {
			rprintf("left\n");
		} else if (setting->val.POS == RIGHT) {
			rprintf("right\n");
		} else if (setting->val.POS == CENTER) {
			rprintf("center\n");
		}
		break;
	}
}

static int parse_setting(struct setting *setting, char *v, int fd)
{
	union value val;

	switch (setting->type) {
	case INT:
		{
			char *end;
			val.INT = strtol(v, &end, 0);

			if (*end != 0) {
				frprintf(rstderr, "Invalid value, expecting integer\n");
				return 1;
			}
		}

		break;
	case BOOL:
		if (strcmp(v, "true") == 0) {
			val.BOOL = 1;
		} else if (strcmp(v, "false") == 0) {
			val.BOOL = 0;
		} else {
			frprintf(rstderr, "Invalid value, expecting boolean\n");
			return 1;
		}
		break;
	case STR:
		val.STR = v;
		break;
	case COL:
		if (*v != '#' || blockbar_parse_color_string(v + 1, val.COL) != 0) {
			frprintf(rstderr, "Invalid color\n");
			return 1;
		}
		break;
	case POS:
		if (strcmp(v, "left") == 0) {
			val.POS = LEFT;
		} else if (strcmp(v, "right") == 0) {
			val.POS = RIGHT;
		} else if (strcmp(v, "center") == 0) {
			val.POS = CENTER;
		} else {
			frprintf(rstderr, "Invalid postion\n");
			return 1;
		}
		break;
	}

	if (set_setting(setting, val)) {
		frprintf(rstderr, "\"%s\" cannot be set to %s\n", setting->name, v);
		return 1;
	}

	return 0;
}

cmd(_get_property)
{
	vars(4, "<property>", 1);

	if (output == -1 && blk->eachmon && strcmp(argv[3], "execdata") == 0) {
		frprintf(rstderr, "Output must be specified when eachmon=true "
 				"for property \"%s\"\n", argv[3]);
		return 1;
	}

	if (strcmp("execdata", argv[3]) == 0) {
		char *exec_data;
		if (blk->eachmon) {
			exec_data = blk->data[output].exec_data;
		} else {
			exec_data = blk->data->exec_data;
		}
		rprintf("%s\n", exec_data ? exec_data : "");

		return 0;
	}

	for (int i = 0; i < property_count; i++) {
		struct setting *property = &((struct setting *) &(blk->properties))[i];

		if (strcmp(argv[3], property->name)) {
			continue;
		}

		print_setting(property, fd);
		return 0;
	}

	frprintf(rstderr, "Property does not exist, or cannot be returned\n");
	return 1;
}

cmd(_set_property)
{
	vars(argc <= 4 ? 0 : argc, "<property> <value>", 1);

	if (output == -1 && blk->eachmon && strcmp(argv[3], "execdata") == 0) {
		frprintf(rstderr, "Output must be specified when eachmon=true "
				"for property \"%s\"\n", argv[3]);
		return 1;
	}

	char str [bbcbuffsize] = {0};

	for (int i = 4; i < argc; i++) {
		strcat(str, argv[i]);
		strcat(str, " ");
	}

	str[strlen(str) - 1] = 0;

	if (strcmp("execdata", argv[3]) == 0) {
		char **exec_data;
		if (blk->eachmon) {
			exec_data = &(blk->data[output].exec_data);
		} else {
			exec_data = &(blk->data->exec_data);
		}

		if (*exec_data) {
			free(*exec_data);
		}

		*exec_data = malloc(strlen(str) + 1);
		strcpy(*exec_data, str);

		goto end;
	} else {
		for (int i = 0; i < property_count; i++) {
			struct setting *property =
				&((struct setting *) &(blk->properties))[i];

			if (strcmp(argv[3], property->name)) {
				continue;
			}

			if (property == &(blk->properties.module)) {
#if _POSIX_C_SOURCE >= 200809L
				char err [bbcbuffsize] = {0};
				FILE *ferr = fmemopen(err, bbcbuffsize, "w");
				int ret = module_register_block(blk, str, ferr);
				fclose(ferr);
				frprintf(rstderr, "%s", err);
#else
				FILE *file = fdopen(fd, "w");
				dprintf(fd, "%c%c", setout, rstdout);
				int ret = module_register_block(blk, str, file);
				fflush(file);
#endif
				if (ret == 0) {
					goto end;
				} else {
					return 1;
				}
			}

			int r = parse_setting(property, str, fd);

			if (r == 0) {
				if (property == &(blk->properties.interval)) {
					update_block_task(blk);
				}

				goto end;
			} else if (r == 1) {
				return 1;
			}
		}
	}

	frprintf(rstderr, "Property does not exist, or cannot be set\n");
	return 1;

end:
	redraw_block(blk);
	redraw();
	return 0;
}

cmd(property)
{
	if (argc == 4) {
		return cmd__get_property(argc, argv, fd);
	} else if (argc >= 5) {
		return cmd__set_property(argc, argv, fd);
	} else {
		frprintf(rstderr, "Usage: %s %s <index>[:output] <property> [value]\n",
				argv[0], argv[1]);
		return 1;
	}
}

cmd(_get_setting)
{
	(void) argc;

	char *colon = strchr(argv[2], ':');

	char *module_name;
	char *setting_name;

	if (colon) {
		module_name = argv[2];
		setting_name = colon + 1;
		*colon = '\0';
	} else {
		module_name = 0;
		setting_name = argv[2];
	}

	if (module_name) {
		struct module *mod = get_module_by_name(module_name);

		if (!mod) {
			frprintf(rstderr, "Module \"%s\" does not exist\n", module_name);
			return 1;
		}

		for (int i = 0; i < mod->data.setting_count; i++) {
			struct setting *setting = &mod->data.settings[i];

			if (strcmp(setting_name, setting->name)) {
				continue;
			}

			print_setting(setting, fd);
			return 0;
		}

		frprintf(rstderr, "Module \"%s\" does not have setting \"%s\"\n",
				module_name, setting_name);

		return 1;
	}

	for (int i = 0; i < setting_count; i++) {
		struct setting *setting = &((struct setting *) &settings)[i];

		if (strcmp(setting_name, setting->name)) {
			continue;
		}

		print_setting(setting, fd);
		return 0;
	}

	frprintf(rstderr, "setting does not exist\n");
	return 1;
}

cmd(_set_setting)
{
	char str [bbcbuffsize] = {0};

	for (int i = 3; i < argc; i++) {
		strcat(str, argv[i]);
		strcat(str, " ");
	}

	str[strlen(str) - 1] = 0;

	char *colon = strchr(argv[2], ':');

	char *module_name;
	char *setting_name;

	if (colon) {
		module_name = argv[2];
		setting_name = colon + 1;
		*colon = '\0';
	} else {
		module_name = 0;
		setting_name = argv[2];
	}

	if (module_name) {
		struct module *mod = get_module_by_name(module_name);

		if (!mod) {
			frprintf(rstderr, "Module \"%s\" does not exist\n", module_name);
			return 1;
		}

		for (int i = 0; i < mod->data.setting_count; i++) {
			struct setting *setting = &mod->data.settings[i];
			if (strcmp(setting_name, setting->name) == 0) {
				int r = parse_setting(setting, str, fd);

				if (r == 0) {
					redraw();

					return 0;
				} else {
					return 1;
				}
			}
		}

		frprintf(rstderr, "Module \"%s\" does not have setting \"%s\"\n",
 				module_name, setting_name);

		return 1;
	}

#define E(x) \
	|| setting == &settings.x

	for (int i = 0; i < setting_count; i++) {
		struct setting *setting = &((struct setting *) &settings)[i];

		if (strcmp(setting_name, setting->name) == 0) {
			int r = parse_setting(setting, str, fd);

			if (r == 0) {
				if (0
					E(height)
					E(marginvert)
					E(marginhoriz)
					E(xoffset)
					E(position)) {
					update_geom();
				}

#ifndef WAYLAND
				if (0
					E(height)
					E(marginhoriz)
					E(padding)
					E(traypadding)
					E(trayiconsize)
					E(trayside)) {
					redraw_tray();
				}

				if (0
					E(background)
					E(traybar)) {
					reparent_icons();
				}
#endif

				for (int j = 0; j < block_count; j++) {
					struct block *blk = &blocks[j];

					if (blk->id) {
						redraw_block(blk);
					}
				}

				redraw();

				return 0;
			} else if (r == 1) {
				return 1;
			}
		}
#undef E
	}

	frprintf(rstderr, "setting does not exist\n");
	return 1;
}

cmd(setting)
{
	if (argc == 3) {
		return cmd__get_setting(argc, argv, fd);
	} else if (argc >= 4) {
		return cmd__set_setting(argc, argv, fd);
	} else {
		frprintf(rstderr, "Usage: %s %s [module:]<setting> [value]\n",
				argv[0], argv[1]);
		return 1;
	}
}

cmd(new)
{
	if (!(argc == 2 || argc == 3)) {
		frprintf(rstderr, "Usage: %s %s [--eachmon]\n", argv[0], argv[1]);
		return 1;
	}

	int eachmon = 0;

	if (argc == 3) {
		if (strcmp(argv[2], "--eachmon")) {
			frprintf(rstderr, "Third argument must be \"--eachmon\" or blank\n");
			return 1;
		}
		eachmon = 1;
	}

	struct block *blk = create_block(eachmon);

	rprintf("%u\n", blk->id);

	return 0;
}

cmd(rm)
{
	vars(3, "", 0);

	remove_block(blk);

	redraw();

	return 0;
}

cmd(move_left)
{
	vars(3, "", 0);

	struct block *swp = 0;
	int i;
	int l;
	int d;

	if (blk->properties.pos.val.POS == RIGHT) {
		i = block_count - 1;
		l = 0;
		d = -1;
	} else {
		i = 0;
		l = block_count;
		d = 1;
	}

	for (; i != l; i += d) {
		struct block *_blk = &blocks[i];
		if (blk->properties.pos.val.POS == _blk->properties.pos.val.POS) {
			if (blk == _blk) {
				break;
			}
			swp = _blk;
		}
	}

	if (swp == 0) {
		frprintf(rstderr, "Cannot move block further left\n");
		return 1;
	}

	struct block tmp;
	memcpy(&tmp, swp, sizeof(struct block));
	memcpy(swp, blk, sizeof(struct block));
	memcpy(blk, &tmp, sizeof(struct block));

	redraw();

	return 0;
}

cmd(move_right)
{
	vars(3, "", 0);

	struct block *swp = 0;
	int i;
	int l;
	int d;

	if (blk->properties.pos.val.POS == RIGHT) {
		i = 0;
		l = block_count;
		d = 1;
	} else {
		i = block_count - 1;
		l = 0;
		d = -1;
	}

	for (; i != l; i += d) {
		struct block *_blk = &blocks[i];
		if (blk->properties.pos.val.POS == _blk->properties.pos.val.POS) {
			if (blk == _blk) {
				break;
			}
			swp = _blk;
		}
	}

	if (swp == 0) {
		frprintf(rstderr, "Cannot move block further right\n");
		return 1;
	}

	struct block tmp;
	memcpy(&tmp, swp, sizeof(struct block));
	memcpy(swp, blk, sizeof(struct block));
	memcpy(blk, &tmp, sizeof(struct block));

	redraw();

	return 0;

}

cmd(dump)
{
	int explicit = 0;

	if (argc == 3) {
		if (strcmp(argv[2], "--explicit")) {
			frprintf(rstderr, "Third argument must be \"--explicit\" or blank\n");
			return 1;
		}
		explicit = 1;
	}

	FILE *file = fdopen(fd, "w");

	dprintf(fd, "%c%c", setout, rstdout);
	char *err = config_save(file, explicit);

	if (err) {
		frprintf(rstderr, "Error dumping config:\n%s\n", err);
		free(err);
		return 1;
	}

	rprintf("\n");

	return 0;
}

cmd(list_modules)
{
	(void) argc;
	(void) argv;

	unsigned int width = 0;

	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];

		if (!mod->dl) {
			continue;
		}

		if (strlen(mod->data.name) > width) {
			width = strlen(mod->data.name);
		}
	}

	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];

		if (!mod->dl) {
			continue;
		}

		rprintf("%-*s%s\n", width + 2, mod->data.name, mod->path)
	}

	return 0;
}

cmd(load_module)
{
	if (argc != 3) {
		frprintf(rstderr, "Usage: %s %s <module file>\n",
				argv[0], argv[1]);
		return 1;
	}

#if _POSIX_C_SOURCE >= 200809L
	char out [bbcbuffsize] = {0};
	char err [bbcbuffsize] = {0};

	FILE *fout = fmemopen(out, bbcbuffsize, "w");
	FILE *ferr = fmemopen(err, bbcbuffsize, "w");

	struct module *ret = load_module(argv[2], -1, fout, ferr);

	fclose(fout);
	fclose(ferr);

	frprintf(rstderr, "%s", err);
	rprintf("%s", out);
#else
	FILE *file = fdopen(fd, "w");
	dprintf(fd, "%c%c", setout, rstdout);
	struct module *ret = load_module(argv[2], -1, file, file);
	fflush(file);
#endif

	return ret == 0;
}

cmd(unload_module)
{
	if (argc != 3) {
		frprintf(rstderr, "Usage: %s %s <module name>\n", argv[0], argv[1]);
		return 1;
	}

	struct module *mod = get_module_by_name(argv[2]);

	if (mod) {
		unload_module(mod);

		for (int i = 0; i < block_count; i++) {
			struct block *blk = &blocks[i];
			if (blk->id) {
				redraw_block(blk);
				redraw();
			}
		}

		rprintf("Module unloaded\n");
		return 0;
	} else {
		frprintf(rstderr, "Module \"%s\" does not exist\n", argv[2]);
		return 1;
	}
}

static void position_module(struct module *mod, int diff, int first)
{
	mod->zindex += diff;
	if (mod->zindex == 0) {
		mod->zindex += diff;
		first = 0;
	}

	for (int i = 0; i < module_count; i++) {
		struct module *_mod = &modules[i];

		if (!_mod->dl) {
			continue;
		}

		if (mod != _mod && mod->zindex == _mod->zindex) {
			mod = _mod;
			position_module(_mod, diff * (first ? -1 : 1), 0);
			break;
		}
	}
}

cmd(raise_lower)
{
	if (argc != 3) {
		frprintf(rstderr, "Usage: %s %s <module name>\n", argv[0], argv[1]);
		return 1;
	}

	struct module *mod = get_module_by_name(argv[2]);

	if (!mod) {
		frprintf(rstderr, "Module \"%s\" does not exist\n", argv[2]);
		return 1;
	}

	if (mod->data.type != RENDER) {
		frprintf(rstderr, "Module \"%s\" is not a render module\n", argv[2]);
		return 1;
	}

	if (strcmp(argv[1], "raise") == 0) {
		position_module(mod, 1, 1);
	} else {
		position_module(mod, -1, 1);
	}

	redraw();

	return 0;
}

#define _CASE(x, y) \
	else if (strcmp(argv[1], x) == 0) { \
		ret = cmd_##y(argc, argv, fd); \
	}

#define CASE(x) \
	_CASE(#x, x)

void socket_recv(int sockfd)
{
	int fd = accept(sockfd, NULL, 0);

	long len = 0;
	char *cmd = 0;

	int argc = 1;
	char **argv;

	char msg [bbcbuffsize];

	int ret;

	if (fd == -1) {
		return;
	}

	struct pollfd fds [] = {
		{fd, POLLIN | POLLHUP, 0},
	};

	while (poll(fds, 1, 5) > 0) {
		if (fds[0].revents & (POLLERR | POLLHUP)) {
			break;
		}

		if (!(fds[0].revents & POLLIN)) {
			continue;
		}

		int n = read(fd, msg, sizeof(msg));

		if (n <= 0) {
			break;
		}

		cmd = realloc(cmd, len + n + 1);
		memcpy(cmd + len, msg, n);

		len += n;

		if (msg[n - 1] == '\x04') {
			len -= 2;
			break;
		}

		cmd[len] = 0;
	}

	if (!cmd) {
		close(fd);
		return;
	}

	for (int i = 0; i < len; i++) {
		if (cmd[i] == 0) {
			argc++;
		}
	}

	argv = malloc(sizeof(char *) * argc);

	argc = 0;

	argv[argc++] = cmd;

	for (int i = 0; i < len; i++) {
		if (cmd[i] == 0) {
			argv[argc++] = cmd + i + 1;
		}
	}

	if (argc < 2) {
		frprintf(rstderr, "No command specified\n");
		ret = 1;
		goto end;
	}

	if (0) {}
	_CASE("--help", help)
	CASE(list)
	CASE(exec)
	_CASE("list-properties", list_properties)
	_CASE("list-settings", list_settings)
	CASE(property)
	CASE(setting)
	CASE(new)
	CASE(rm)
	_CASE("move-left", move_left)
	_CASE("move-right", move_right)
	CASE(dump)
	_CASE("list-modules", list_modules)
	_CASE("load-module", load_module)
	_CASE("unload-module", unload_module)
	_CASE("raise", raise_lower)
	_CASE("lower", raise_lower)
	else {
		frprintf(rstderr, "Unknown command\n");
		ret = 1;
	}

end:
	dprintf(fd, "%c%c", setret, ret);

	free(cmd);
	free(argv);
	close(fd);
}
