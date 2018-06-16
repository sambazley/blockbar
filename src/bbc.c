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

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char msg [BBCBUFFSIZE] = {0};

    char *dest = msg;
    for (int i = 0; i < argc; i++) {
        if (dest + strlen(argv[i]) - msg >= BBCBUFFSIZE - 1) {
            fprintf(stderr, "Input too long\n");
            return 1;
        }

        dest += sprintf(dest, "%s ", argv[i]);
    }

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        return 1;
    }

    struct sockaddr_un sockAddr;

    sockAddr.sun_family = AF_UNIX;
    strcpy(sockAddr.sun_path, SOCKETPATH);

    if (connect(sockfd, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) == -1) {
        fprintf(stderr, "Error connecting to socket\n");
        return 1;
    }

    if (send(sockfd, msg, dest-msg-1, 0) == -1) {
        fprintf(stderr, "Error sending data\n");
        return 1;
    }

    struct pollfd fds [] = {
        {sockfd, POLLIN, 0},
        {STDOUT_FILENO, POLLHUP, 0},
    };

    char rsp [BBCBUFFSIZE];
    int n, ret = 0;

    while (poll(fds, 2, -1) > 0) {
        if (fds[1].revents & (POLLERR | POLLHUP)) {
            break;
        }
        if (fds[0].revents & POLLIN) {
            if ((n = recv(sockfd, rsp, sizeof(rsp), 0)) > 0) {
                rsp[n] = '\0';

                printf("%s", rsp+1);
                fflush(stdout);
                ret = rsp[0];
            } else {
                break;
            }
        }
    }

    close(sockfd);

    return ret;
}
