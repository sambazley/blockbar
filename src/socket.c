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
#include "blocks.h"
#include "config.h"
#include "exec.h"
#include "render.h"
#include "tray.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int socketInit() {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        return -1;
    }

    struct sockaddr_un sockAddr;

    sockAddr.sun_family = AF_UNIX;
    strcpy(sockAddr.sun_path, SOCKETPATH);

    unlink(SOCKETPATH);
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

#define rprintf(fmt, ...) \
    sprintf(rsp + strlen(rsp), fmt, ##__VA_ARGS__)

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


static int help(int argc, char **argv, char *rsp) {
#define phelp(key, val) \
    rprintf("\t%-27s%s\n", key, val)

    rprintf("Usage: %s <command>\n\n", argv[0]);
    rprintf("Commands:\n");
    phelp("list", "List blocks by their indices and \"exec\" value");
    phelp("exec <n>", "Execute block's script");
    phelp("list-properties", "List a block's properties");
    phelp("list-settings", "List the bar's settings");
    phelp("property <n>[:o] <p> [v]", "Gets or sets a property of a block");
    phelp("setting <p> [v]", "Gets or sets a setting of the bar");
    phelp("new [eachmon]", "Creates a new block");
    phelp("rm <n>", "Removes a block");
    phelp("move-out <n>", "Moves a block towards the outside of the bar");
    phelp("move-in <n>", "Moves a block towards the inside of the bar");

#undef phelp

    return 0;
}

static int list(int argc, char **argv, char *rsp) {
    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        if (blk->id) {
            rprintf("%u\t%s\n", blk->id, blocks[i].exec);
        }
    }

    return 0;
}

static int exec(int argc, char **argv, char *rsp) {
    vars(3, "", 0);

    blockExec(blk, 0);
    return 0;
}

static int list_properties(int argc, char **argv, char *rsp) {
#define p(t, v, d) \
    rprintf("%-8s%-17s%s\n", t, v, d);

    p("string", "mode", "Block mode (\"legacy\" or \"subblocks\")");
    p("string", "label", "Block's label");
    p("string", "exec", "Script to be executed");
    p("string", "execdata", "Data that is displayed");
    p("string", "pos", "Position of the block (\"left\" or \"right\")");
    p("int", "interval", "Time between each execution of the block's script");
    p("int", "padding", "Padding on both sides of the block");
    p("int", "padding-inside", "Additional padding on the inside of the block");
    p("int", "padding-outside", "Additional padding on the outside of the block");
    p("bool", "nodiv", "Disables the outside divider next to the block");

#undef p

    return 0;
}

static int list_settings(int argc, char **argv, char *rsp) {
#define p(t, v, d) \
    rprintf("%-8s%-15s%s\n", t, v, d);

    p("int", "height", "Height of the bar");
    p("int", "padding", "Padding on both sides of each block");
    p("color", "background", "Background color of the bar");
    p("color", "foreground", "Default text color");
    p("string", "font", "Font name and size");
    p("bool", "shortlabels", "Whether a block's label should render in short mode");
    p("string", "position", "Position of the bar on the screen (\"top\" or \"bottom\")");
    p("int", "traypadding", "Padding to the right of each tray icon");
    p("int", "trayiconsize", "Width and height of each tray icon");
    p("string", "traybar", "Name of the output that the tray appears on");
    p("string", "trayside", "Side that the tray appears on the bar (\"left\" or \"right\")");

#undef p

    return 0;
}

static int getProperty(int argc, char **argv, char *rsp) {
    vars(4, "<property>", 1);

    if (output == -1 && blk->eachmon && strcmp(argv[3], "execdata") == 0) {
        rprintf("Output must be specified when eachmon=true "
                "for property \"%s\"\n", argv[3]);
        return 1;
    }

#define IS(x) \
    else if (strcmp(argv[3], x) == 0)

    if (0) {}
    IS("mode") {
        rprintf("%s\n", blk->mode == LEGACY ? "legacy" : "subblocks");
    }
    IS("label") {
        rprintf("%s\n", blk->label ? blk->label : "");
    }
    IS("exec") {
        rprintf("%s\n", blk->exec ? blk->exec : "");
    }
    IS("execdata") {
        char *execData;
        if (blk->eachmon) {
            execData = blk->data.mon[output].type.legacy.execData;
        } else {
            execData = blk->data.type.legacy.execData;
        }
        rprintf("%s\n", execData ? execData : "");
    }
    IS("pos") {
        rprintf("%s\n", blk->pos == LEFT ? "left" : "right");
    }
    IS("interval") {
        rprintf("%d\n", blk->interval);
    }
    IS("padding") {
        rprintf("%d\n", blk->padding);
    }
    IS("padding-inside") {
        rprintf("%d\n", blk->padIn);
    }
    IS("padding-outside") {
        rprintf("%d\n", blk->padOut);
    }
    IS("nodiv") {
        rprintf("%s\n", blk->nodiv ? "true" : "false");
    }
    else {
        rprintf("Property does not exist, or cannot be returned\n");
        return 1;
    }
#undef IS
    return 0;
}

static int setProperty(int argc, char **argv, char *rsp) {
    vars(argc <= 4 ? 0 : argc, "<property> <value>", 1);

    if (output == -1 && blk->eachmon && strcmp(argv[3], "execdata") == 0) {
        rprintf("Output must be specified when eachmon=true "
                "for property \"%s\"\n", argv[3]);
        return 1;
    }

    char val [BBCBUFFSIZE] = {0};

    for (int i = 4; i < argc; i++) {
        strcat(val, argv[i]);
        strcat(val, " ");
    }

    val[strlen(val) - 1] = 0;

#define IS(x) \
    else if (strcmp(argv[3], x) == 0)

#define INT \
    char *end; \
    int integer = strtol(val, &end, 0); \
    if (*end != 0) { \
        rprintf("Invalid value, expecting integer\n"); \
        return 1; \
    }

    if (0) {}
    IS("mode") {
        if (strcmp("legacy", val) == 0) {
            blk->mode = LEGACY;
        } else if (strcmp("subblocks", val) == 0) {
            blk->mode = SUBBLOCK;
        } else {
            rprintf("Invalid mode\n");
            return 1;
        }
    }
    IS("label") {
        if (blk->label) {
            free(blk->label);
        }
        blk->label = malloc(strlen(val) + 1);
        strcpy(blk->label, val);
    }
    IS("exec") {
        if (blk->exec) {
            free(blk->exec);
        }
        blk->exec = malloc(strlen(val) + 1);
        strcpy(blk->exec, val);
    }
    IS("execdata") {
        char **execData;
        if (blk->eachmon) {
            execData = &(blk->data.mon[output].type.legacy.execData);
        } else {
            execData = &(blk->data.type.legacy.execData);
        }

        if (*execData) {
            free(*execData);
        }

        *execData = malloc(strlen(val) + 1);
        strcpy(*execData, val);
    }
    IS("pos") {
        if (strcmp("left", val) == 0) {
            blk->pos = LEFT;
        } else if (strcmp("right", val) == 0) {
            blk->pos = RIGHT;
        } else {
            rprintf("Invalid position\n");
            return 1;
        }
    }
    IS("interval") {
        INT;
        blk->interval = integer;
        updateTickInterval();
    }
    IS("padding") {
        INT;
        blk->padding = integer;
    }
    IS("padding-inside") {
        INT;
        blk->padIn = integer;
    }
    IS("padding-outside") {
        INT;
        blk->padOut = integer;
    }
    IS("nodiv") {
        if (strcmp("true", val) == 0) {
            blk->nodiv = 1;
        } else if (strcmp("false", val) == 0) {
            blk->nodiv = 0;
        } else {
            rprintf("Invalid value, expecting boolean "
                    "(\"true\" or \"false\")\n");
        }
    }
    else {
        rprintf("Property does not exist, or cannot be set\n");
        return 1;
    }

#undef INT
#undef IS

    redraw();

    return 0;
}

static int property(int argc, char **argv, char *rsp) {
    if (argc == 4) {
        return getProperty(argc, argv, rsp);
    } else if (argc >= 5) {
        return setProperty(argc, argv, rsp);
    } else {
        rprintf("Usage: %s %s <index>[:output] <property> [value]\n",
                argv[0], argv[1]);
        return 1;
    }
}

static int getSetting(int argc, char **argv, char *rsp) {
#define IS(x) \
    else if (strcmp(argv[2], x) == 0)

    if (0) {}
    IS("height") {
        rprintf("%d\n", conf.height);
    }
    IS("padding") {
        rprintf("%d\n", conf.padding);
    }
    IS("background") {
        rprintf("#%02x%02x%02x\n", conf.bg[0], conf.bg[1], conf.bg[2]);
    }
    IS("foreground") {
        rprintf("#%02x%02x%02x\n", conf.fg[0], conf.fg[1], conf.fg[2]);
    }
    IS("font") {
        rprintf("%s\n", conf.font);
    }
    IS("shortlabels") {
        rprintf("%s\n", conf.shortLabels ? "true" : "false");
    }
    IS("position") {
        rprintf("%s\n", conf.top ? "top" : "bottom");
    }
    IS("traypadding") {
        rprintf("%d\n", conf.trayPadding);
    }
    IS("trayiconsize") {
        rprintf("%d\n", conf.trayIconSize);
    }
    IS("traybar") {
        rprintf("%s\n", conf.trayBar);
    }
    IS("trayside") {
        rprintf("%s\n", conf.traySide == RIGHT ? "right" : "left");
    }
    else {
        rprintf("Setting does not exist, or cannot be returned\n");
        return 1;
    }

#undef IS

    return 0;
}

static int setSetting(int argc, char **argv, char *rsp) {
    char val [BBCBUFFSIZE] = {0};

    for (int i = 3; i < argc; i++) {
        strcat(val, argv[i]);
        strcat(val, " ");
    }

    val[strlen(val) - 1] = 0;

#define IS(x) \
    else if (strcmp(argv[2], x) == 0)

#define INT \
    char *end; \
    int integer = strtol(val, &end, 0); \
    if (*end != 0) { \
        rprintf("Invalid value, expecting integer\n"); \
        return 1; \
    }

    if (0) {}
    IS("height") {
        INT;
        conf.height = integer;
        updateGeom();
        redrawTray();
    }
    IS("padding") {
        INT;
        conf.padding = integer;
    }
    IS("background") {
        if ((strlen(val) != 4 && strlen(val) != 7) || *val != '#' ||
            parseColorString(val+1, conf.bg) != 0) {
            rprintf("Invalid color\n");
            return 1;
        }
        reparentIcons();
    }
    IS("foreground") {
        if ((strlen(val) != 4 && strlen(val) != 7) || *val != '#' ||
            parseColorString(val+1, conf.fg) != 0) {
            rprintf("Invalid color\n");
            return 1;
        }
    }
    IS("font") {
        if (conf.font) {
            free(conf.font);
        }

        conf.font = malloc(strlen(val) + 1);
        strcpy(conf.font, val);
        renderInit();
    }
    IS("shortlabels") {
        if (strcmp(val, "true") == 0) {
            conf.shortLabels = 1;
        } else if (strcmp(val, "false") == 0) {
            conf.shortLabels = 0;
        } else {
            rprintf("Invalid value, expecting boolean "
                    "(\"true\" or \"false\")\n");
            return 1;
        }
    }
    IS("position") {
        if (strcmp(val, "top") == 0) {
            conf.top = 1;
        } else if (strcmp(val, "bottom") == 0) {
            conf.top = 0;
        } else {
            rprintf("Invalid value, expecting position "
                    "(\"top\" or \"bottom\")\n");
            return 1;
        }
        updateGeom();
    }
    IS("traypadding") {
        INT;
        conf.trayPadding = integer;
        redrawTray();
    }
    IS("traybar") {
        for (int i = 0; i < barCount; i++) {
            struct Bar bar = bars[i];
            if (strcmp(bar.output, val) == 0) {
                trayInit(i);
                break;
            }
        }
        reparentIcons();
    }
    IS("trayiconsize") {
        INT;
        conf.trayIconSize = integer;
        redrawTray();
    }
    IS("trayside") {
        if (strcmp(val, "left") == 0) {
            conf.traySide = LEFT;
        } else if (strcmp(val, "right") == 0) {
            conf.traySide = RIGHT;
        } else {
            rprintf("Invalid value, expecting side "
                    "(\"left\" or \"right\")\n");
            return 1;
        }
        redrawTray();
    }
    else {
        rprintf("Setting does not exist, or cannot be set\n");
    }

#undef INT
#undef IS

    redraw();

    return 0;
}

static int setting(int argc, char **argv, char *rsp) {
    if (argc == 3) {
        return getSetting(argc, argv, rsp);
    } else if (argc >= 4) {
        return setSetting(argc, argv, rsp);
    } else {
        rprintf("Usage: %s %s <property> [value]\n", argv[0], argv[1]);
        return 1;
    }
}

static int new(int argc, char **argv, char *rsp) {
    if (!(argc == 2 || argc == 3)) {
        rprintf("Usage: %s %s [eachmon]\n", argv[0], argv[1]);
        return 1;
    }

    int eachmon = 0;

    if (argc == 3) {
        if (strcmp(argv[2], "eachmon")) {
            rprintf("Third argument must be \"eachmon\" or blank\n");
            return 1;
        }
        eachmon = 1;
    }

    struct Block *blk = createBlock(eachmon);

    rprintf("%u\n", blk->id);

    return 0;
}

static int rm(int argc, char **argv, char *rsp) {
    vars(3, "", 0);

    removeBlock(blk);

    return 0;
}

static int move_out(int argc, char **argv, char *rsp) {
    vars(3, "", 0);

    struct Block *swp = 0;
    for (int i = 0; i < blockCount; i++) {
        struct Block *_blk = &blocks[i];
        if (blk->pos == _blk->pos) {
            if (blk == _blk) {
                break;
            }
            swp = _blk;
        }
    }

    if (swp == 0) {
        rprintf("Cannot move block out further\n");
        return 1;
    }

    struct Block tmp;
    memcpy(&tmp, swp, sizeof(struct Block));
    memcpy(swp, blk, sizeof(struct Block));
    memcpy(blk, &tmp, sizeof(struct Block));

    redraw();

    return 0;
}

static int move_in(int argc, char **argv, char *rsp) {
    vars(3, "", 0);

    struct Block *swp = 0;
    for (int i = 0; i < blockCount; i++) {
        struct Block *_blk = &blocks[i];
        if (_blk == blk) {
            for (i++; i < blockCount; i++) {
                struct Block *_blk = &blocks[i];
                if (blk->pos == _blk->pos) {
                    swp = _blk;
                    break;
                }
            }
            break;
        }
    }

    if (swp == 0) {
        rprintf("Cannot move block in further\n");
        return 1;
    }

    struct Block tmp;
    memcpy(&tmp, swp, sizeof(struct Block));
    memcpy(swp, blk, sizeof(struct Block));
    memcpy(blk, &tmp, sizeof(struct Block));

    redraw();

    return 0;

}

#define _CASE(x, y) \
    else if (strcmp(argv[1], x) == 0) { \
        rsp[0] = y(argc, argv, rsp+1); \
    }

#define CASE(x) \
    _CASE(#x, x)

void socketRecv(int sockfd) {
    int fd = accept(sockfd, NULL, 0);
    char msg [BBCBUFFSIZE];
    int n;

    if (fd > 0 && (n = recv(fd, msg, sizeof(msg), 0)) > 0) {
        msg[n] = '\0';

        char rsp [BBCBUFFSIZE] = {0};

        int argc = 1;

        for (int i = 0; i < n; i++) {
            if (msg[i] == ' ') {
                argc++;
            }
        }

        char **argv = malloc(sizeof(char *) * argc);

        argc = 0;
        argv[argc++] = msg;

        for (int i = 0; i < n; i++) {
            if (msg[i] == ' ') {
                msg[i] = 0;
                argv[argc++] = msg + i + 1;
            }
        }

        if (argc < 2) {
            rprintf("%cNo command specified\n", 1);
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
        _CASE("move-out", move_out)
        _CASE("move-in", move_in)
        else {
            rprintf("%cUnknown command\n", 1);
        }

end:
        free(argv);

        if (send(fd, rsp, sizeof(rsp), MSG_NOSIGNAL) == -1) {
            fprintf(stderr, "Error sending response\n");
        }
        close(fd);
    }
}
