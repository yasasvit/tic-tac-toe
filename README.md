# Tic-Tac-Toe
Names: Pratyoy Biswas, Yasasvi Tallapaneni  

## Description
Our code was based on `echoservt.c`.  

To represent the game, we created a `struct game` to represent and store the necessary information related to the game. This struct contains all information including ports numbers, names, socket file descriptors, and roles (`'X'` and `'O'`) for both players, as well as the tic-tac-toe board, number of players, the current turn, and whether or not there is a draw offered. There is also a pointer to the next game, allowing the game structure to be treated like a linked list.

Since the games list is a global variable, we must lock it before makes any changes to the games. We accomplish this my using `mutex`. When a command is sent to the server and has passed basic validity checks, we use `pthread_mutex_lock` to lock the games list (`dummyg`). After the message is processed and either executed or rejected as invalid, it is unlocked with `pthread_mutex_unlock`.

## Test Plan
To check each message sent to the server, we first split the message into its parts: the command, the length, and the arguments. If the given command does not match a command that can be sent by the client or the given length does not match the length of the arguments, we reject it and send an INVL message to the client. Then, the rest of the arguments are parsed based on the command. If the arguments are invalid for that command, we send an INVL message back to the client.

We tested every scenario for which there could be invalid message formats, so all invalid message formats should be caught. Scenarios tested for PLAY include:
1. Player is already in game.
2. Player's desired name is already taken.
3. If there are no games with only 1 player, a new game is created. If there is a game with only 1 player, then the new player is added to that game and the game begins by sending a BEGN message to both players.

Scenarios for MOVE include:
1. Player's role does not match. (INVL)
2. The position is not on the board. (INVL)
3. It is not the player's turn. (INVL)
4. The player is not in an active game. (INVL)
5. The square is already occupied. (INVL)
6. Move is successful, but it does not lead to a win and there are still empty squares. (MOVD)
7. Move is successful and leads to a win for that player. (MOVD, OVER)
8. Move is successful, but it does not lead to a win and there are no more empty squares, so it is a draw. (MOVD, OVER)
9. The turn is changed when a move is successful, and not changed when it is not successful.

Scenarios for RSGN include:
1. Player is not in an active game. (INVL)
2. Player successfully resigns when it is their turn, and OVER is sent to both players. (OVER)
3. Player successfully resigns when it is not their turn, and OVER is sent to both players, and the game is deleted from the list. (OVER)

Scenarios for DRAW include:
1. Player is not in an active game. (INVL)
2. Player attempts to accept a draw when none was suggested by the opponent. (INVL)
3. Player attempts to reject a draw when none was suggested by the opponent. (INVL)
4. Player attempts to suggest a draw when the opponent already suggested one. (INVL)
5. Player attempts to suggest a draw when none was suggested by the opponent. (DRAW)
6. Player accepts a draw that was suggested by the opponent. (OVER)
7. Player rejects a draw that was suggested by the opponent. (DRAW|2|R|)
8. All of the above cases when it is not the current player's turn.

Scenarios for unexpectedly terminated clients (e.g., EOF):
1. Terminated player was in an active game, so their opponent is granted a win and the game is deleted.
2. If the terminated player was not in an active game, nothing needs to be done.
