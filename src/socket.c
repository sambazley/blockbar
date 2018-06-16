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
    rprintf("\t%-20s%s\n", key, val)

static int help(int argc, char **argv, char *rsp) {
    rprintf("Usage: %s <command>\n\n", argv[0]);
    rprintf("Commands:\n");
    phelp("list", "List blocks by their indices and \"exec\" value");
    phelp("exec <index>", "Execute block's script");
    return 0;
}

static int list(int argc, char **argv, char *rsp) {
    for (int i = 0; i < blockCount; i++) {
        rprintf("%u\t%s\n", i, blocks[i].exec);
    }

    return 0;
}

static int exec(int argc, char **argv, char *rsp) {
    if (argc != 3) {
        rprintf("Usage: %s %s <block index>\n", argv[0], argv[1]);
        return 1;
    }

    char *end;
    int index = strtol(argv[2], &end, 0);

    if (*end != 0) {
        rprintf("Invalid index, expecting integer\n");
        return 1;
    }

    if (index >= blockCount) {
        rprintf("No block at index %u\n", index);
        return 1;
    }

    blockExec(&blocks[index], 0);
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

        argv[0] = strtok(msg, " ");
        for (int i = 1; i < argc; i++) {
            argv[i] = strtok(NULL, " ");
        }
        strtok(NULL, " ");

        if (argc < 2) {
            rprintf("%cNo command specified\n", 1);
            goto end;
        }


        if (0) {}
        _CASE("--help", help)
        CASE(list)
        CASE(exec)
        else {
            rprintf("%cUnknown command\n", 1);
        }

end:
        free(argv);

        if (send(fd, rsp, sizeof(rsp), 0) == -1) {
            fprintf(stderr, "Error sending response\n");
        }
        close(fd);
    }
}
