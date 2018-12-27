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
#include "tray.h"
#include "util.h"
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int socketInit() {
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

    struct sockaddr_un sockAddr;

    sockAddr.sun_family = AF_UNIX;
    strcpy(sockAddr.sun_path, socketpath);

    unlink(socketpath);
    if (bind(sockfd, (struct sockaddr *) &sockAddr, sizeof(sockAddr)) == -1) {
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
    static int x(int argc, char **argv, int fd)

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
        char *outputName = end + 1; \
        if (strcmp(outputName, "*") == 0) { \
            output = -1; \
        } \
        for (int i = 0; i < barCount && output == -2; i++) { \
            struct Bar *bar = &bars[i]; \
            if (strcmp(outputName, bar->output) == 0) { \
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
    struct Block *blk = getBlock(index); \
    if (!blk) { \
        rprintf("No block at index %u\n", index); \
        return 1; \
    }

cmd(help) {
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

#undef phelp

    return 0;
}

cmd(list) {
    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        if (blk->id) {
            rprintf("%u\t%s\n", blk->id, blocks[i].properties.exec.val.STR);
        }
    }

    return 0;
}

cmd(exec) {
    vars(3, "", 0);

    blockExec(blk, 0);
    return 0;
}

cmd(list_properties) {
#define p(t, v, d) \
    rprintf("%-9s%-17s%s\n", t, v, d);

    for (int i = 0; i < propertyCount; i++) {
        struct Setting *property = &((struct Setting *) &defProperties)[i];
        p(typeStrings[property->type], property->name, property->desc);
    }

    p("string", "execdata", "Data that is displayed");
#undef p

    return 0;
}

cmd(list_settings) {
    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];
        rprintf("%-9s%-15s%s\n", typeStrings[setting->type], setting->name,
                setting->desc);
    }

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (mod->data.settingCount == 0) {
            continue;
        }

        rprintf("\n%s:\n", mod->data.name);

        for (int j = 0; j < mod->data.settingCount; j++) {
            struct Setting *setting = &mod->data.settings[j];
            rprintf("%-9s%-15s%s\n", typeStrings[setting->type], setting->name,
                    setting->desc);
        }
    }

    return 0;
}

static int printSetting(struct Setting *setting, char *str, int fd) {
    if (strcmp(str, setting->name) == 0) {
        switch (setting->type) {
        case INT:
            rprintf("%d\n", setting->val.INT);
            return 0;
        case BOOL:
            rprintf("%s\n", setting->val.BOOL ? "true" : "false");
            return 0;
        case STR:
            rprintf("%s\n", setting->val.STR);
            return 0;
        case COL:
            rprintf("#%02x%02x%02x%02x\n",
                    setting->val.COL[0],
                    setting->val.COL[1],
                    setting->val.COL[2],
                    setting->val.COL[3]);
            return 0;
        case POS:
            if (setting->val.POS == LEFT) {
                rprintf("left\n");
            } else if (setting->val.POS == RIGHT) {
                rprintf("right\n");
            } else if (setting->val.POS == CENTER) {
                rprintf("center\n");
            }
            return 0;
        }
    }
    return 1;
}

static int parseSetting(struct Setting *setting, char *v, int fd) {
    union Value val;

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
        if (*v != '#' || parseColorString(v + 1, val.COL) != 0) {
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

    if (setSetting(setting, val)) {
        frprintf(rstderr, "\"%s\" cannot be set to %s\n", setting->name, v);
        return 1;
    }

    return 0;
}

cmd(_getProperty) {
    vars(4, "<property>", 1);

    if (output == -1 && blk->eachmon && strcmp(argv[3], "execdata") == 0) {
        frprintf(rstderr, "Output must be specified when eachmon=true "
                 "for property \"%s\"\n", argv[3]);
        return 1;
    }

    if (strcmp("execdata", argv[3]) == 0) {
        char *execData;
        if (blk->eachmon) {
            execData = blk->data[output].execData;
        } else {
            execData = blk->data->execData;
        }
        rprintf("%s\n", execData ? execData : "");

        return 0;
    }

    for (int i = 0; i < propertyCount; i++) {
        struct Setting *property = &((struct Setting *) &(blk->properties))[i];

        if (printSetting(property, argv[3], fd) == 0) {
            return 0;
        }
    }

    frprintf(rstderr, "Property does not exist, or cannot be returned\n");
    return 1;
}

cmd(_setProperty) {
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
        char **execData;
        if (blk->eachmon) {
            execData = &(blk->data[output].execData);
        } else {
            execData = &(blk->data->execData);
        }

        if (*execData) {
            free(*execData);
        }

        *execData = malloc(strlen(str) + 1);
        strcpy(*execData, str);

        goto end;
    } else {
        for (int i = 0; i < propertyCount; i++) {
            struct Setting *property =
                &((struct Setting *) &(blk->properties))[i];

            if (strcmp(argv[3], property->name) == 0) {
                if (property == &(blk->properties.module)) {
                    moduleUnregisterBlock(blk);
                }

                int r = parseSetting(property, str, fd);

                if (r == 0) {
                    if (property == &(blk->properties.module)) {
                        moduleRegisterBlock(blk);
                    } else if (property == &(blk->properties.interval)) {
                        updateTickInterval();
                    }

                    goto end;
                } else if (r == 1) {
                    return 1;
                }
            }
        }
    }

    frprintf(rstderr, "Property does not exist, or cannot be set\n");
    return 1;

end:
    redraw();
    return 0;
}

cmd(property) {
    if (argc == 4) {
        return _getProperty(argc, argv, fd);
    } else if (argc >= 5) {
        return _setProperty(argc, argv, fd);
    } else {
        frprintf(rstderr, "Usage: %s %s <index>[:output] <property> [value]\n",
                 argv[0], argv[1]);
        return 1;
    }
}

cmd(_getSetting) {
    char *colon = strchr(argv[2], ':');

    char *moduleName;
    char *settingName;

    if (colon) {
        moduleName = argv[2];
        settingName = colon + 1;
        *colon = '\0';
    } else {
        moduleName = 0;
        settingName = argv[2];
    }

    if (moduleName) {
        struct Module *mod = 0;

        for (int i = 0; i < moduleCount; i++) {
            mod = &modules[i];
            if (strcmp(mod->data.name, moduleName) == 0) {
                break;
            }
            mod = 0;
        }

        if (mod == 0) {
            frprintf(rstderr, "Module \"%s\" does not exist\n", moduleName);
            return 1;
        }

        for (int i = 0; i < mod->data.settingCount; i++) {
            struct Setting *setting = &mod->data.settings[i];

            if (printSetting(setting, settingName, fd) == 0) {
                return 0;
            }
        }

        frprintf(rstderr, "Module \"%s\" does not have setting \"%s\"\n",
                 moduleName, settingName);

        return 1;
    }

    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];

        if (printSetting(setting, settingName, fd) == 0) {
            return 0;
        }
    }

    frprintf(rstderr, "Setting does not exist\n");
    return 1;
}

cmd(_setSetting) {
    char str [bbcbuffsize] = {0};

    for (int i = 3; i < argc; i++) {
        strcat(str, argv[i]);
        strcat(str, " ");
    }

    str[strlen(str) - 1] = 0;

    char *colon = strchr(argv[2], ':');

    char *moduleName;
    char *settingName;

    if (colon) {
        moduleName = argv[2];
        settingName = colon + 1;
        *colon = '\0';
    } else {
        moduleName = 0;
        settingName = argv[2];
    }

    if (moduleName) {
        struct Module *mod = 0;

        for (int i = 0; i < moduleCount; i++) {
            mod = &modules[i];
            if (strcmp(mod->data.name, moduleName) == 0) {
                break;
            }
            mod = 0;
        }

        if (mod == 0) {
            frprintf(rstderr, "Module \"%s\" does not exist\n", moduleName);
            return 1;
        }

        for (int i = 0; i < mod->data.settingCount; i++) {
            struct Setting *setting = &mod->data.settings[i];
            if (strcmp(settingName, setting->name) == 0) {
                int r = parseSetting(setting, str, fd);

                if (r == 0) {
                    redraw();

                    return 0;
                } else {
                    return 1;
                }
            }
        }

        frprintf(rstderr, "Module \"%s\" does not have setting \"%s\"\n",
                 moduleName, settingName);

        return 1;
    }

#define E(x) \
    || setting == &settings.x

    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];

        if (strcmp(settingName, setting->name) == 0) {
            int r = parseSetting(setting, str, fd);

            if (r == 0) {
                if (0
                    E(height)
                    E(marginvert)
                    E(marginhoriz)
                    E(xoffset)
                    E(position)) {
                    updateGeom();
                }

                if (0
                    E(height)
                    E(marginhoriz)
                    E(traypadding)
                    E(trayiconsize)
                    E(trayside)) {
                    redrawTray();
                }

                if (0
                    E(background)
                    E(traybar)) {
                    reparentIcons();
                }

                redraw();

                return 0;
            } else if (r == 1) {
                return 1;
            }
        }
#undef E
    }

    frprintf(rstderr, "Setting does not exist\n");
    return 1;
}

cmd(setting) {
    if (argc == 3) {
        return _getSetting(argc, argv, fd);
    } else if (argc >= 4) {
        return _setSetting(argc, argv, fd);
    } else {
        frprintf(rstderr, "Usage: %s %s [module:]<setting> [value]\n",
                 argv[0], argv[1]);
        return 1;
    }
}

cmd(new) {
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

    struct Block *blk = createBlock(eachmon);

    rprintf("%u\n", blk->id);

    return 0;
}

cmd(rm) {
    vars(3, "", 0);

    removeBlock(blk);

    redraw();

    return 0;
}

cmd(move_left) {
    vars(3, "", 0);

    struct Block *swp = 0;
    int i;
    int l;
    int d;

    if (blk->properties.pos.val.POS == RIGHT) {
        i = blockCount - 1;
        l = 0;
        d = -1;
    } else {
        i = 0;
        l = blockCount;
        d = 1;
    }

    for (; i != l; i += d) {
        struct Block *_blk = &blocks[i];
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

    struct Block tmp;
    memcpy(&tmp, swp, sizeof(struct Block));
    memcpy(swp, blk, sizeof(struct Block));
    memcpy(blk, &tmp, sizeof(struct Block));

    redraw();

    return 0;
}

cmd(move_right) {
    vars(3, "", 0);

    struct Block *swp = 0;
    int i;
    int l;
    int d;

    if (blk->properties.pos.val.POS == RIGHT) {
        i = 0;
        l = blockCount;
        d = 1;
    } else {
        i = blockCount - 1;
        l = 0;
        d = -1;
    }

    for (; i != l; i += d) {
        struct Block *_blk = &blocks[i];
        if (blk->properties.pos.val.POS == _blk->properties.pos.val.POS) {
            if (blk == _blk) {
                break;
            }
            swp = _blk;
        }
    }

    if (swp == 0) {
        frprintf(rstderr, "Cannot move block in further\n");
        return 1;
    }

    struct Block tmp;
    memcpy(&tmp, swp, sizeof(struct Block));
    memcpy(swp, blk, sizeof(struct Block));
    memcpy(blk, &tmp, sizeof(struct Block));

    redraw();

    return 0;

}

cmd(dump) {
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
    char *err = configSave(file, explicit);

    if (err) {
        frprintf(rstderr, "Error dumping config:\n%s\n", err);
        free(err);
        return 1;
    }

    rprintf("\n");

    return 0;
}

cmd(list_modules) {
    int width = 0;

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl) {
            continue;
        }

        if (strlen(mod->data.name) > width) {
            width = strlen(mod->data.name);
        }
    }

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        rprintf("%-*s%s\n", width + 2, mod->data.name, mod->path)
    }

    return 0;
}

cmd(load_module) {
    if (argc != 3) {
        frprintf(rstderr, "Usage: %s %s <module file>\n", argv[0], argv[1]);
        return 1;
    }

#if _POSIX_C_SOURCE >= 200809L
    char out [bbcbuffsize] = {0};
    char err [bbcbuffsize] = {0};

    FILE *fout = fmemopen(out, bbcbuffsize, "w");
    FILE *ferr = fmemopen(err, bbcbuffsize, "w");

    struct Module *ret = loadModule(argv[2], fout, ferr);

    fclose(fout);
    fclose(ferr);

    frprintf(rstderr, "%s", err);
    rprintf("%s", out);
#else
    dprintf(fd, "%c%c", setout, rstdout);
    struct Module *ret = loadModule(argv[2], file, file);
    fflush(file);
#endif

    return ret != 0;
}

cmd(unload_module) {
    if (argc != 3) {
        frprintf(rstderr, "Usage: %s %s <module name>\n", argv[0], argv[1]);
        return 1;
    }

    int ret = unloadModule(argv[2]);

    if (ret == 0) {
        rprintf("Module unloaded\n");
    } else {
        frprintf(rstderr, "Module \"%s\" does not exist\n", argv[2]);
    }

    return ret;
}

#define _CASE(x, y) \
    else if (strcmp(argv[1], x) == 0) { \
        ret = y(argc, argv, fd); \
    }

#define CASE(x) \
    _CASE(#x, x)

void socketRecv(int sockfd) {
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
