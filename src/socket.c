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
#include "exec.h"
#include "render.h"
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

#define phelp(key, val) \
    rprintf("\t%-22s%s\n", key, val)

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
    if (index >= blockCount) { \
        rprintf("No block at index %u\n", index); \
        return 1; \
    }


static int help(int argc, char **argv, char *rsp) {
    rprintf("Usage: %s <command>\n\n", argv[0]);
    rprintf("Commands:\n");
    phelp("list", "List blocks by their indices and \"exec\" value");
    phelp("exec <n>", "Execute block's script");
    phelp("list-props", "List a block's properties");
    phelp("get <n>[:o] <p>", "Get a property of a block");
    phelp("set <n>[:o] <p> <v>", "Set a property of a block");
    return 0;
}

static int list(int argc, char **argv, char *rsp) {
    for (int i = 0; i < blockCount; i++) {
        rprintf("%u\t%s\n", i, blocks[i].exec);
    }

    return 0;
}

static int exec(int argc, char **argv, char *rsp) {
    vars(3, "", 0);

    blockExec(&blocks[index], 0);
    return 0;
}

static int list_props(int argc, char **argv, char *rsp) {
#define p(t, v, d) \
    rprintf("%-10s%-20s%s\n", t, v, d);

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

static int get(int argc, char **argv, char *rsp) {
    vars(4, "<property>", 1);

    struct Block *blk = &blocks[index];

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
        rprintf("%u\n", blk->interval);
    }
    IS("padding") {
        rprintf("%u\n", blk->padding);
    }
    IS("padding-inside") {
        rprintf("%u\n", blk->padIn);
    }
    IS("padding-outside") {
        rprintf("%u\n", blk->padOut);
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

static int set(int argc, char **argv, char *rsp) {
    vars(argc <= 4 ? 0 : argc, "<property> <value>", 1);

    struct Block *blk = &blocks[index];

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
            rprintf("Invalid value, expecting boolean"
                    "(\"true\" or \"false\")\n");
        }
    }
    else {
        rprintf("Property does not exist, or cannot be set\n");
        return 1;
    }

#undef INT
#undef IS

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
        _CASE("list-props", list_props)
        CASE(get)
        CASE(set)
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
