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

#include "bbc.h"
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        return 1;
    }

    char *socketpath = getenv("BLOCKBAR_SOCKET");

    if (!socketpath) {
        socketpath = defsocketpath;
    }

    struct sockaddr_un sockAddr;

    sockAddr.sun_family = AF_UNIX;
    strcpy(sockAddr.sun_path, socketpath);

    if (connect(sockfd, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) == -1) {
        fprintf(stderr, "Error connecting to socket\n");
        return 1;
    }

    for (int i = 0; i < argc; i++) {
        if (send(sockfd, argv[i], strlen(argv[i]) + 1, 0) == -1) {
            fprintf(stderr, "Error sending data\n");
            return 1;
        }
    }

    send(sockfd, "\x04", 1, 0);

    struct pollfd fds [] = {
        {sockfd, POLLIN, 0},
        {STDOUT_FILENO, POLLHUP, 0},
    };

    FILE *out = stdout;
    char rsp [bbcbuffsize];
    int n, ret = 0;

    while (poll(fds, 2, -1) > 0) {
        if (fds[1].revents & (POLLERR | POLLHUP)) {
            break;
        }
        if (fds[0].revents & POLLIN) {
            if ((n = recv(sockfd, rsp, sizeof(rsp), 0)) > 0) {
                rsp[n] = 0;
                for (int i = 0; i < n; i++) {
                    if (rsp[i] == setout) {
                        fflush(out);
                        if (rsp[i + 1] == rstdout) {
                            out = stdout;
                        } else if (rsp[i + 1] == rstderr) {
                            out = stderr;
                        }
                        i++;
                    } else if (rsp[i] == setret) {
                        ret = rsp[i + 1];
                        i++;
                    } else {
                        fprintf(out, "%c", rsp[i]);
                    }
                }
            } else {
                break;
            }
        }
    }

    fflush(out);

    close(sockfd);

    return ret;
}
