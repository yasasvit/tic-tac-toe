// NOTE: must use option -pthread when compiling!
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>

#define QUEUE_SIZE 8

volatile int active = 1;

struct game {
    char * port1;
    char * port2;
    int fd1;
    int fd2;
    char * name1;
    char * name2;
    char role1;
    char role2;
    char board[3][3];
    int size;
    struct game* next;
    char turn; // 'X'
    char draw;
};

struct game* dummyg;

void handler(int signum)
{
    active = 0;
}

// set up signal handlers for primary thread
// return a mask blocking those signals for worker threads
// FIXME should check whether any of these actually succeeded
void install_handlers(sigset_t *mask)
{
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    
    sigemptyset(mask);
    sigaddset(mask, SIGINT);
    sigaddset(mask, SIGTERM);
}

// data to be sent to worker threads
struct connection_data {
	struct sockaddr_storage addr;
	socklen_t addr_len;
	int fd;
};

pthread_mutex_t mutex;

int open_listener(char *service, int queue_size)
{
    struct addrinfo hint, *info_list, *info;
    int error, sock;

    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family   = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags    = AI_PASSIVE;

    // obtain information for listening socket
    error = getaddrinfo(NULL, service, &hint, &info_list);
    if (error) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    // attempt to create socket
    for (info = info_list; info != NULL; info = info->ai_next) {
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

        // if we could not create the socket, try the next method
        if (sock == -1) continue;

        // bind socket to requested port
        error = bind(sock, info->ai_addr, info->ai_addrlen);
        if (error) {
            close(sock);
            continue;
        }

        // enable listening for incoming connection requests
        error = listen(sock, queue_size);
        if (error) {
            close(sock);
            continue;
        }

        // if we got this far, we have opened the socket
        break;
    }

    freeaddrinfo(info_list);

    // info will be NULL if no method succeeded
    if (info == NULL) {
        fprintf(stderr, "Could not bind.\n");
        return -1;
    }

    return sock;
}

bool check(char role, char board[3][3])  {
    // check rows
    for (int i = 0; i < 3; i++) {
        bool allSame = true;
        for (int j = 0; j < 3; j++) {
            if (board[i][j] != role) {
                allSame = false;
            }
        }
        if (allSame) {
            return true;
        }
    }
    // check cols
    for (int j = 0; j < 3; j++) {
        bool allSame = true;
        for (int i = 0; i < 3; i++) {
            if (board[i][j] != role) {
                allSame = false;
            }
        }
        if (allSame) {
            return true;
        }
    }
    // check two diagonals
    bool allSame = true;
    for (int i = 0; i < 3; i++) {
        if (board[i][i] != role) {
            allSame = false;
        }
    }
    if (allSame) {
        return true;
    }

    allSame = true;
    for (int i = 0; i < 3; i++) {
        if (board[2 - i][i] != role) {
            allSame = false;
        }
    }
    if (allSame) {
        return true;
    }
    return false;
} 

bool checkDraw(char board[3][3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (board[i][j] == '.')
                return false;
        }
    }
    return true;
}

#define BUFSIZE 256
#define HOSTSIZE 100
#define PORTSIZE 10
void *read_data(void *arg)
{
	struct connection_data *con = arg;
    char buf[BUFSIZE + 1], host[HOSTSIZE], port[PORTSIZE];
    int bytes, error;

    // int msgcount = 0;

    error = getnameinfo(
    	(struct sockaddr *)&con->addr, con->addr_len,
    	host, HOSTSIZE,
    	port, PORTSIZE,
    	NI_NUMERICSERV
    );
    if (error) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(error));
        strcpy(host, "??");
        strcpy(port, "??");
    }

    printf("Connection from %s:%s\n", host, port);
    // struct game* g = (struct game*) malloc(sizeof(struct game));
    // // fill board w placeholders
    // for (int i = 0; i < 3; i++) {
    //     for (int j = 0; j < 3; j++) {
    //         g->board[i][j] = '.';
    //     }
    // }

    while (active && (bytes = recv(con->fd, buf, BUFSIZE, 0)) > 0) {
        buf[bytes-1] = '\0';
        bytes--;
        printf("[%s:%s] read %d bytes |%s|\n", host, port, bytes, buf);
        // msgcount++;
        // char num = msgcount + 'A' - 1;
        // char* s = malloc(2);
        // s[0] = num;
        // s[1] = '\0';
        // char* msg = (char *) malloc(50);
        // strcpy(msg, "Message received (\0");
        // strcat(msg, s);
        // strcat(msg, ")\n\0");

        // send(con->fd, msg, 22, 0);
        // free(s);
        // free(msg);
        //
        // WORKING ON DATA
        
        if (bytes < 7 || buf[4] != '|') {
            printf("Invalid message\n");
            send(con->fd, "INVL|12|Bad message|", 20, 0);
            continue;
        }

        // int index = 0;
        // while (index < strlen(buf) && buf[index] != '|') {
        //     index += 1;
        // }
        // // Means client typed invalid command because no '|' in buf
        // if (index >= strlen(buf)) {
        //     // ERROR
        // }
        // char cmd[index + 1];
        // strncpy(cmd, buf, index);
        int index = 4;
        char cmd[index + 1];
        strncpy(cmd, buf, index);
        cmd[index] = '\0';
        // printf("Command: %s.\n", cmd);
        int start = index + 1;
        index += 1;
        // while (index < strlen(buf) && buf[index] != '|') {
        //     index += 1;
        // }
        while (index < strlen(buf) && (buf[index] >= '0' && buf[index] <= '9')) {
            index += 1;
        }
        if (index >= strlen(buf) || buf[index] != '|') {
            // ERROR
            //free(cmd);
            printf("Invalid length\n");
            send(con->fd, "INVL|11|Bad length|", 19, 0);
            continue;  
        }
        int countTotal = 0;
        char* count = malloc(index-start+1);
        for (int i = 0; i < start ; i++) {
            countTotal += 1;
        }
        int i1 = 0;
        while (start != index) {
            count[i1] = buf[start];
            i1 += 1;
            start += 1;
        }
        count[i1] = '\0';
        index += 1;
        for (int i = 0; i < start ; i++) {
            countTotal += 1;
        }
        int c = atoi(count);
        free(count);
        printf("Length: %d.\n", c);
        if (c != bytes - index) {
            send(con->fd, "INVL|11|Bad length|", 19, 0);
            printf("Invalid length\n");
            continue;
        }
        pthread_mutex_lock(&mutex);
        if (strcmp(cmd, "PLAY") == 0) {
            //c -= 1;
            char* name = malloc(c);
            int i2 = 0;
            int startTest = 0; 
            while (i2 < c-1) {
                if (buf[index] == '|') break;
                name[i2] = buf[index];
                i2 += 1;
                index += 1;
                // c -= 1;
            }
            for (int i = 0; i < 100; i++) {
                startTest += 1;
            }
            if (i2 != c-1) {
                printf("Mismatched Length\n");
                send(con->fd, "INVL|11|Bad length|", 19, 0);
                free(name);
                pthread_mutex_unlock(&mutex);
                continue;
            }
            name[i2] = '\0';
            printf("Name: %s.\n", name);
            bool foundGame = false;
            bool foundName = false;

            bool foundPort = false;
            struct game* runner = dummyg->next;
            struct game* prev = dummyg;
            while (runner) {
                if (strcmp(runner->name1, name) == 0 || (runner->size == 2 && strcmp(runner->name2, name) == 0)) {
                    foundName = true;
                    send(con->fd, "INVL|17|Name unavailable|", 25, 0);
                    printf("Name in use\n");
                    break;
                }
                if (strcmp(runner->port1, port) == 0 || (runner->size == 2 && strcmp(runner->port2, port) == 0)) {
                    foundPort = true;
                    send(con->fd, "INVL|16|Already in game|", 24, 0);
                    printf("Already playing\n");
                    break;
                }
                if (runner->size == 1) {
                    runner->port2 = (char*) malloc(strlen(port)+1);
                    int operations = 100;
                    int counter = 10;
                    do {
                        operations -= 1;
                        counter -= 1;
                    } while (counter > 0);
                    strcpy(runner->port2, port);
                    runner->port2[strlen(port)] = '\0';
                    runner->name2 = (char*) malloc(strlen(name)+1);
                    strcpy(runner->name2, name);
                    runner->name2[strlen(name)] = '\0';
                    runner->fd2 = con->fd;
                    runner->size = 2;
                    // Set board
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            runner->board[i][j] = '.';
                        }
                    }
                    runner->role1 = 'X';
                    runner->role2 = 'O';
                    runner->turn = 'X';
                    runner->draw = '-';
                    // Set roles and send begin
                    foundGame = true;
                    printf("Starting Game: %s vs. %s\n", runner->name1, runner->name2);
                    send(con->fd, "WAIT|0|", 7, 0); // send WAIT
                    int k1 = snprintf(NULL, 0, "%d", (int) strlen(runner->name2)+3);
                    int temporaryStore1 = k1;
                    do {
                        printf("%d\n", temporaryStore1); 
                        temporaryStore1 -= 1;
                    } while (temporaryStore1 > 0);
                    char* begn1 = malloc(10 + k1 + strlen(runner->name2));
                    operations = 100;
                    counter = 10;
                    do {
                        operations -= 1;
                        counter -= 1;
                    } while (counter > 0);
                    snprintf(begn1, 10+k1+strlen(runner->name2), "BEGN|%d|%c|%s|", (int) strlen(runner->name2)+3, runner->role1, runner->name2);
                    printf("%s\n", begn1);
                    send(runner->fd1, begn1, strlen(begn1), 0);
                    int k2 = snprintf(NULL, 0, "%d", (int) strlen(runner->name1)+3);
                    int temporaryStore = k2;
                    do {
                        printf("%d\n", temporaryStore); 
                        temporaryStore -= 1;
                    } while (temporaryStore > 0);
                    char* begn2 = malloc(10 + k2 + strlen(runner->name1));
                    snprintf(begn2, 10+k1+strlen(runner->name1), "BEGN|%d|%c|%s|", (int) strlen(runner->name1)+3, runner->role2, runner->name1);
                    printf("%s\n", begn2);
                    send(runner->fd2, begn2, strlen(begn2), 0);
                    free(name);
                    free(begn1);
                    free(begn2);
                    // SEND BEGN
                    break;
                } else {
                    prev = runner;
                    runner = runner->next;
                }
                
            }
            if (foundName || foundPort) {
                free(name);
                pthread_mutex_unlock(&mutex);
                continue;
            } if (!foundGame) {
                int portsCreated = 2;
                printf("Create new game\n");
                struct game* newGame = (struct game*) (malloc(sizeof(struct game)));
                newGame->size = 1;
                newGame->port1 = (char*) malloc(strlen(port)+1);
                portsCreated -= 1;
                // strcpy(runner->port1, port);
                // runner->name1 = (char*) malloc(strlen(name));
                // strcpy(runner->name1, name);
                strcpy(newGame->port1, port);
                newGame->port1[strlen(port)] = '\0';
                newGame->name1 = (char*) malloc(strlen(name)+1);
                strcpy(newGame->name1, name);
                newGame->name1[strlen(name)] = '\0';
                portsCreated -= 1;
                newGame->fd1 = con->fd;
                newGame->next = NULL;
                do {
                    // SHOULD NEVER PRINT WHILE RUNNING
                    // IF PRINTS, THERE IS ERROR
                    printf("%d\n", portsCreated);
                    portsCreated -= 1;
                } while (portsCreated > 0);
                prev->next = newGame;
                send(con->fd, "WAIT|0|", 7, 0); // send WAIT
                free(name);

            }


            
        }
        //  else if (strcmp(cmd, "BEGN") == 0) {
        //     char r = buf[index];
        //     index += 2;
        //     c -= 3;
        //     char* name = malloc(100);
        //     int i2 = 0;
        //     while (index < strlen(buf) && c > 0) {
        //         name[i2] = buf[index];
        //         i2 += 1;
        //         index += 1;
        //         c -= 1;
        //     }
        //     if (strcmp(name, g->player1) == 0) {
        //         g->role1 = r;
        //     } else if (strcmp(name, g->player2) == 0){
        //         g->role2 = r;
        //     } else {
        //         // What do we do if role does not match any name
        //     }
        //     free(name); 
        // } 
        else if (strcmp(cmd, "MOVE") == 0) {
            int countErrors = 0;
            if (c != 6) {
                printf("Invalid move\n");
                countErrors += 1;
                send(con->fd, "INVL|13|Invalid Move|", 21, 0);
                while (countErrors > 0) {
                    // printf("%d", countErrors);
                    countErrors -= 1;
                }
                pthread_mutex_unlock(&mutex);
                continue;
            }
            char r = buf[index];
            printf("Letter: %c\n", r);
            index += 1;
            if (buf[index] != '|') {
                printf("Invalid move\n");
                countErrors += 1;
                send(con->fd, "INVL|13|Invalid Move|", 21, 0);
                while (countErrors > 0) {
                    // printf("%d", countErrors);
                    countErrors -= 1;
                }
                pthread_mutex_unlock(&mutex);
                continue;
            }
            index += 1;
            if (buf[index] < '1' && buf[index] > '3') {
                printf("Invalid row\n");
                countErrors += 1;
                send(con->fd, "INVL|18|Row out of bounds|", 26, 0);
                while (countErrors > 0) {
                    // printf("%d", countErrors);
                    countErrors -= 1;
                }
                pthread_mutex_unlock(&mutex);
                continue;
            }
            int row = buf[index] - '1';
            index += 1;
            if (buf[index] != ',') {
                printf("Invalid format\n");
                countErrors += 1;
                send(con->fd, "INVL|15|Invalid format|", 23, 0);
                while (countErrors > 0) {
                    // printf("%d", countErrors);
                    countErrors -= 1;
                }
                pthread_mutex_unlock(&mutex);
                continue;
            }
            index += 1;
            if (buf[index] < '1' && buf[index] > '3') {
                printf("Invalid column\n");
                countErrors += 1;
                send(con->fd, "INVL|21|Column out of bounds|", 29, 0);
                while (countErrors > 0) {
                    // printf("%d", countErrors);
                    countErrors -= 1;
                }
                pthread_mutex_unlock(&mutex);
                continue;
            }
            int col = buf[index] - '1';
            index += 1;
            if (buf[index] != '|') {
                printf("Invalid move\n");
                countErrors += 1;
                send(con->fd, "INVL|13|Invalid Move|", 21, 0);
                while (countErrors > 0) {
                    // printf("%d", countErrors);
                    countErrors -= 1;
                }
                pthread_mutex_unlock(&mutex);
                continue;
            }
            // Find game
            struct game* runner = dummyg->next;
            while (runner) {
                if (strcmp(runner->port1, port) == 0 || (runner->size == 2 && strcmp(runner->port2, port) == 0)) {
                    break;
                }
                runner = runner->next;
            }
            if (!runner || runner->size == 1) {
                printf("No game\n");
                send(con->fd, "INVL|15|No Active Game|", 23, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }
            
            if (r != runner->turn || (strcmp(runner->port1, port) == 0 && r != runner->role1) || (strcmp(runner->port2, port) == 0 && r != runner->role2)){
                printf("Not your turn\n");
                send(con->fd, "INVL|14|Not your turn|", 22, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            if (runner->board[row][col] == '.') {
                runner->board[row][col] = r;
                runner->turn = r == 'X' ? 'O' : 'X';
                runner->draw = '-';
            } else {
                printf("Space occupied\n");
                send(con->fd, "INVL|23|Space already occupied|", 31, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }
            int countPlacements = 0;
            char * b = malloc(10);
            int i10 = 0;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    printf("%c", runner->board[i][j]);
                    countPlacements += 1;
                    b[i10++] = runner->board[i][j];
                }
                printf("\n");
            }
            b[9] = '\0';
            do {
                // printf("%d ", countPlacements);
                countPlacements -= 1;
            } while (countPlacements > 0); 
            
            bool gameover = false;
            // MOVD|16|X|2,2|....X....|
            char * movd1 = malloc(25);
            snprintf(movd1, 25, "MOVD|16|%c|%d,%d|%s|", r, row+1, col+1, b);
            printf("%s\n", movd1);
            send(runner->fd1, movd1, strlen(movd1), 0);
            send(runner->fd2, movd1, strlen(movd1), 0);
            free(movd1);
            free(b);
            if (check(r, runner->board)) {
                // Player WONN
                // What to do?
                if (runner->turn == 'O') {
                    printf("Over: %s wins\n", runner->name1);
                    send(runner->fd1, "OVER|22|W|You got 3 in a row.|", 30, 0);
                    send(runner->fd2, "OVER|27|L|Opponent got 3 in a row.|", 35, 0);

                    // OVER|22|W|You got 3 in a row.|
                } else {
                    printf("Over: %s wins\n", runner->name2);
                    send(runner->fd2, "OVER|22|W|You got 3 in a row.|", 30, 0);
                    send(runner->fd1, "OVER|27|L|Opponent got 3 in a row.|", 35, 0);
                }
                gameover = true;
            } else if (checkDraw(runner->board)) {
                printf("Over: draw\n");
                send(runner->fd1, "OVER|17|D|Board is full.|", 25, 0);
                send(runner->fd2, "OVER|17|D|Board is full.|", 25, 0);
                gameover = true;
            }

            if (gameover) {
                struct game* prev = dummyg;
                while (prev && prev->next) {
                    if (strcmp(prev->next->port1, port) == 0 || (prev->next->size == 2 && strcmp(prev->next->port2, port) == 0)) {
                        break;
                    }
                    prev = prev->next;
                }
                if (!prev) {
                    pthread_mutex_unlock(&mutex);
                    continue;
                }

                struct game* freetemp = prev->next;
                prev->next = prev->next->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
            }

            //
            // MOVD
        } else if (strcmp(cmd, "RSGN") == 0) {
            if (c != 0) {
                printf("Invalid resign\n");
                send(con->fd, "INVL|15|Invalid Resign|", 23, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            struct game* prev = dummyg;
            while (prev && prev->next) {
                if (strcmp(prev->next->port1, port) == 0 || (prev->next->size == 2 && strcmp(prev->next->port2, port) == 0)) {
                    break;
                }
                prev = prev->next;
            }

            if (!prev || !(prev->next) || prev->next->size == 1) {
                printf("Invalid resign\n");
                send(con->fd, "INVL|15|Invalid Resign|", 23, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }
            struct game* runner = dummyg->next;
            while (runner) {
                if (strcmp(port, runner->port1) == 0) {
                    printf("Over: %s wins\n", runner->name2);
                    send(runner->fd1, "OVER|16|L|You resigned.|", 24, 0);
                    break;
                    //
                    send(runner->fd2, "OVER|25|W|Opponent has resigned.|", 33, 0);
                } else if (strcmp(port, runner->port2) == 0) {
                    printf("Over: %s wins\n", runner->name1);
                    send(runner->fd2, "OVER|16|L|You resigned.|", 24, 0);
                    send(runner->fd1, "OVER|25|W|Opponent has resigned.|", 33, 0);
                    break;
                }
                runner = runner->next;
            }

            struct game* freetemp = prev->next;
            prev->next = prev->next->next;
            free(freetemp->name1);
            free(freetemp->port1);
            if (freetemp->size == 2) free(freetemp->name2);
            if (freetemp->size == 2) free(freetemp->port2);
            free(freetemp);
            
            // OVER
        } else if (strcmp(cmd, "DRAW") == 0) {
            if (c != 2 && buf[index+1] != '|') {
                printf("invalid draw\n");
                send(con->fd, "INVL|13|Invalid Draw|", 21, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            char offer = buf[index];
            printf("Offer: %c\n", offer);
            if (offer != 'A' && offer != 'S' && offer != 'R') {
                printf("invalid draw\n");
                send(con->fd, "INVL|13|Invalid Draw|", 21, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            

            struct game* prev = dummyg;
            while (prev && prev->next) {
                if (strcmp(prev->next->port1, port) == 0 || (prev->next->size == 2 && strcmp(prev->next->port2, port) == 0)) {
                    break;
                }
                prev = prev->next;
            }

            if (!prev || !(prev->next) || prev->next->size == 1) {
                printf("invalid draw\n");
                send(con->fd, "INVL|13|Invalid Draw|", 21, 0);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            if (strcmp(prev->next->port1, port) == 0) {
                if (offer == 'A' && prev->next->draw == 'O') {
                    printf("Over: draw\n");
                    send(prev->next->fd1, "OVER|16|D|Draw accepted|", 24, 0); // casework
                    send(prev->next->fd2, "OVER|16|D|Draw accepted|", 24, 0);
                    struct game* freetemp = prev->next;
                    prev->next = prev->next->next;
                    free(freetemp->name1);
                    free(freetemp->port1);
                    if (freetemp->size == 2) free(freetemp->name2);
                    if (freetemp->size == 2) free(freetemp->port2);
                    free(freetemp);
                } else if (offer == 'S' && prev->next->draw == '-') {
                    prev->next->draw = 'X';
                    printf("Over: draw suggested\n");
                    send(prev->next->fd2, "DRAW|2|S|", 9, 0);
                } else if (offer == 'R' && prev->next->draw == 'O') {
                    prev->next->draw = '-';
                    printf("Over: rejected\n");
                    send(prev->next->fd2, "DRAW|2|R|", 9, 0);
                } else {
                    printf("invalid draw\n");
                    send(con->fd, "INVL|13|Invalid Draw|", 21, 0);
                }
            } else if (strcmp(prev->next->port2, port) == 0) {
                if (offer == 'A' && prev->next->draw == 'X') {
                    printf("Over: draw\n");
                    send(prev->next->fd1, "OVER|16|D|Draw accepted|", 24, 0); // casework
                    send(prev->next->fd2, "OVER|16|D|Draw accepted|", 24, 0);
                    struct game* freetemp = prev->next;
                    prev->next = prev->next->next;
                    free(freetemp->name1);
                    free(freetemp->port1);
                    if (freetemp->size == 2) free(freetemp->name2);
                    if (freetemp->size == 2) free(freetemp->port2);
                    free(freetemp);
                } else if (offer == 'S' && prev->next->draw == '-') {
                    printf("Over: suggested\n");
                    prev->next->draw = 'O';
                    send(prev->next->fd1, "DRAW|2|S|", 9, 0);
                } else if (offer == 'R' && prev->next->draw == 'X') {
                    printf("Over: draw rejected\n");
                    prev->next->draw = '-';
                    send(prev->next->fd1, "DRAW|2|R|", 9, 0);
                } else {
                    printf("invalid draw\n");
                    send(con->fd, "INVL|13|Invalid Draw|", 21, 0);
                }
            }

        } else {
            printf("invalid command\n");
            send(con->fd, "INVL|16|Invalid Command|", 24, 0);
        }
        pthread_mutex_unlock(&mutex);
        // END OF WORKING ON DATA
        // 
        //send(con->fd, "Message received (", 18, 0);
        //send(con->fd, &num, 1, 0);
        // free(cmd);
        // free(count);
    }
	if (bytes == 0) {
		printf("[%s:%s] got EOF\n", host, port);
        struct game* prev = dummyg;
        while (prev && prev->next) {
            if (strcmp(port, prev->next->port1) == 0) {
                printf("Over: %s wins.\n", prev->next->name2);
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                send(prev->next->fd2, "OVER|23|W|Opponent disconnected.|", 31, 0);
                struct game* freetemp = prev->next;
                prev->next = prev->next->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
                break;
            } else if (strcmp(port, prev->next->port2) == 0) {
                printf("Over: %s wins.\n", prev->next->name1);
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                send(prev->next->fd1, "OVER|23|W|Opponent disconnected.|", 31, 0);
                struct game* freetemp = prev->next;
                prev->next = prev->next->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
                break;
            }
            prev = prev->next;
        }

	} else if (bytes == -1) {
		printf("[%s:%s] terminating: %s\n", host, port, strerror(errno));
        struct game* prev = dummyg;
        while (prev && prev->next) {
            if (strcmp(port, prev->next->port1) == 0) {
                printf("Over: %s wins.\n", prev->next->name2);
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                send(prev->next->fd2, "OVER|23|W|Opponent disconnected.|", 31, 0);
                struct game* freetemp = prev->next;
                prev->next = prev->next->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
                break;
            } else if (strcmp(port, prev->next->port2) == 0) {
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                printf("Over: %s wins.\n", prev->next->name1);
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                send(prev->next->fd1, "OVER|23|W|Opponent disconnected.|", 31, 0);
                struct game* freetemp = prev->next;
                prev->next = prev->next->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
                break;
            }
            prev = prev->next;
        }

	} else {
		printf("[%s:%s] terminating\n", host, port);
        struct game* prev = dummyg;
        while (prev && prev->next) {
            if (strcmp(port, prev->next->port1) == 0) {
                printf("Over: %s wins.\n", prev->next->name2);
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                send(prev->next->fd2, "OVER|23|W|Opponent disconnected.|", 31, 0);
                struct game* freetemp = prev->next;
                prev->next = prev->next->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
                break;
            } else if (strcmp(port, prev->next->port2) == 0) {
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                printf("Over: %s wins.\n", prev->next->name1);
                //send(runner->fd1, "OVER|20|L|Your game crashed|", 28, 0);
                send(prev->next->fd1, "OVER|23|W|Opponent disconnected.|", 31, 0);
                struct game* freetemp = prev->next;
                prev->next = prev->next->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
                break;
            }
            prev = prev->next;
        }

	}

    close(con->fd);
    free(con);
    
    return NULL;
}

int main(int argc, char **argv)
{
    sigset_t mask;
    struct connection_data *con;
    int error;
    pthread_t tid;
    pthread_mutex_init(&mutex, NULL);

    if (argc != 2) {
        printf("Please specify port number.\n");
        exit(1);
    }
    char *service = argv[1];

	install_handlers(&mask);
	
    int listener = open_listener(service, QUEUE_SIZE);
    if (listener < 0) exit(EXIT_FAILURE);
    
    printf("Listening for incoming connections on %s\n", service);

    dummyg = (struct game*) (malloc(sizeof(struct game)));
    dummyg->size = 2;
    dummyg->next = NULL;
    while (active) {
    	con = (struct connection_data *)malloc(sizeof(struct connection_data));
    	con->addr_len = sizeof(struct sockaddr_storage);
    
        con->fd = accept(listener, 
            (struct sockaddr *)&con->addr,
            &con->addr_len);

        if (con->fd < 0) {
            perror("accept");
            struct game* freeptr = dummyg->next;
            while (freeptr) {
                struct game* freetemp = freeptr;
                freeptr = freeptr->next;
                free(freetemp->name1);
                free(freetemp->port1);
                if (freetemp->size == 2) free(freetemp->name2);
                if (freetemp->size == 2) free(freetemp->port2);
                free(freetemp);
            }
            free(dummyg);
            free(con);
            // TODO check for specific error conditions
            continue;
        }
         
        //send(con->fd, "Connected\n", 10, 0);

        // temporarily disable signals
        // (the worker thread will inherit this mask, ensuring that SIGINT is
        // only delivered to this thread)
        error = pthread_sigmask(SIG_BLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	exit(EXIT_FAILURE);
        }
        
        error = pthread_create(&tid, NULL, read_data, con);
        if (error != 0) {
        	fprintf(stderr, "pthread_create: %s\n", strerror(error));
        	close(con->fd);
        	free(con);
        	continue;
        }
        
        // automatically clean up child threads once they terminate
        pthread_detach(tid);
        
        // unblock handled signals
        error = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	exit(EXIT_FAILURE);
        }

    }

    pthread_mutex_destroy(&mutex);

    puts("Shutting down");
    close(listener);
    
    // returning from main() (or calling exit()) immediately terminates all
    // remaining threads

    // to allow threads to run to completion, we can terminate the primary thread
    // without calling exit() or returning from main:
    //   pthread_exit(NULL);
    // child threads will terminate once they check the value of active, but
    // there is a risk that read() will block indefinitely, preventing the
    // thread (and process) from terminating
    
	// to get a timely shut-down of all threads, including those blocked by
	// read(), we will could maintain a global list of all active thread IDs
	// and use pthread_cancel() or pthread_kill() to wake each one

    return EXIT_SUCCESS;
}
