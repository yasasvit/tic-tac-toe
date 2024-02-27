#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

int connect_inet(char *host, char *service)
{
    struct addrinfo hints, *info_list, *info;
    int sock, error;

    // look up remote host
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;  // in practice, this means give us IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // indicate we want a streaming socket

    error = getaddrinfo(host, service, &hints, &info_list);
    if (error) {
        fprintf(stderr, "error looking up %s:%s: %s\n", host, service, gai_strerror(error));
        return -1;
    }

    for (info = info_list; info != NULL; info = info->ai_next) {
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (sock < 0) continue;

        error = connect(sock, info->ai_addr, info->ai_addrlen);
        if (error) {
            close(sock);
            continue;
        }

        break;
    }
    freeaddrinfo(info_list);

    if (info == NULL) {
        fprintf(stderr, "Unable to connect to %s:%s\n", host, service);
        return -1;
    }

    return sock;
}

void endGame(char res, char* expl) {
    write(STDIN_FILENO, "Game over: ", 11);
    if (res == 'W') write(STDIN_FILENO, "You win. ", 9);
    else if (res == 'L') write(STDIN_FILENO, "You lose. ", 10);
    else write(STDIN_FILENO, "Draw. ", 6);
    write(STDIN_FILENO, expl, strlen(expl));
    write(STDIN_FILENO, "\n", 1);
}

#define BUFLEN 256

int main(int argc, char **argv)
{
    int sock, bytes;
    char buf[BUFLEN];

    if (argc != 3) {
        printf("Specify host and service\n");
        exit(EXIT_FAILURE);
    }

    sock = connect_inet(argv[1], argv[2]);
    if (sock < 0) exit(EXIT_FAILURE);


    // bytes = recv(sock, buf, BUFLEN, 0);
    // write(STDIN_FILENO, "Server: ", 8);
    // write(STDIN_FILENO, buf, bytes);

    while ((bytes = read(STDIN_FILENO, buf, BUFLEN)) > 0) {
        // // START GAME:
        // do {
        //     write(STDIN_FILENO, "Name: ", 6);
        // } while ((bytes = read(STDIN_FILENO, buf, BUFLEN)) > 1);
        // buf[bytes-1] = '\0';
        // int k = snprintf(NULL, 0, "%d", bytes);
        // char* msg = malloc(bytes+k+7);
        // snprintf(msg, bytes+k+7, "PLAY|%d|%s|", bytes, buf);
        // send(sock, msg, bytes+k+7, 0);
        // free(msg);

        // bytes = recv(sock, buf, BUFLEN, 0);
        // if (strcmp(buf, "WAIT|0|") != 0) {
        //     printf("Server error\n");
        // }

        // bytes = recv(sock, buf, BUFLEN, 0);
        // if (strncmp(buf, "OVER", 4) == 0) {
        //     int start = 5, index = 5;
        //     while (index < bytes && (buf[index] >= '0' && buf[index] <= '9')) {index += 1;}
        //     char* count = malloc(index-start+1);
        //     int i1 = 0;
        //     while (start != index) {
        //         count[i1] = buf[start];
        //         i1 += 1;
        //         start += 1;
        //     }
        //     count[i1] = '\0';
        //     int c = atoi(count);
        //     free(count);

        //     char res = buf[index+1];
        //     char* expl = malloc(c-2);
        //     index += 3;
        //     for (int i = index; i < index + c-3; i++) {
        //         expl[i-index] = buf[i];
        //     }
        //     expl[c-3] = '\0';
        //     endGame(res, expl);
        //     free(expl);
        //     continue;
        // } else if (strcmp(buf, "WAIT|0|") != 0) {
        //     printf("Server error\n");
        // }

        // bytes = recv(sock, buf, BUFLEN, 0);
        // if (strncmp(buf, "OVER", 4) == 0) {
        //     int start = 5, index = 5;
        //     while (index < bytes && (buf[index] >= '0' && buf[index] <= '9')) {index += 1;}
        //     char* count = malloc(index-start+1);
        //     int i1 = 0;
        //     while (start != index) {
        //         count[i1] = buf[start];
        //         i1 += 1;
        //         start += 1;
        //     }
        //     count[i1] = '\0';
        //     int c = atoi(count);
        //     free(count);

        //     char res = buf[index+1];
        //     char* expl = malloc(c-2);
        //     index += 3;
        //     for (int i = index; i < index + c-3; i++) {
        //         expl[i-index] = buf[i];
        //     }
        //     expl[c-3] = '\0';
        //     endGame(res, expl);
        //     free(expl);
        //     continue;
        // } 

        send(sock, buf, bytes, 0);
        //bytes = recv(sock, buf, BUFLEN, 0);
        //write(STDIN_FILENO, "Server: ", 8);
        //write(STDIN_FILENO, buf, bytes);
        //if (bytes == 0 || buf[bytes-1] != '\n') write(STDIN_FILENO, "\n", 1);
        // FIXME: should check whether the write succeeded!
    }

    close(sock);

    return EXIT_SUCCESS;
}
