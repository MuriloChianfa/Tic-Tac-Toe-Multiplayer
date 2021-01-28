#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <sys/types.h>

#include "customSTD.h"

#define MAX_CLIENTS 10
#define MAX_ROOMS 5
#define BUFFER_SZ 2048
#define NAME_LEN 64

static _Atomic unsigned int cli_count = 0; 
static int uid = 10;
static int roomUid = 1;

int posicoes[9][2] = {{2, 0}, {2, 1}, {2, 2}, {1, 0}, {1, 1}, {1, 2}, {0, 0}, {0, 1}, {0, 2}};

// client structure
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[NAME_LEN];
} client_t;

typedef struct {
    char tabuleiro[3][3];
    int estadoDeJogo;
    int rodada;
    int turnoDoJogador;
} game_t;

// room structure
typedef struct {
    client_t *player1;
    client_t *player2;
    unsigned int uid;
    char state[NAME_LEN];
    game_t *game;
} room_t;

client_t *clients[MAX_CLIENTS];
room_t *rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

#include "queueManager.h"

void send_message(char *message, int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i])
        {   
            if (clients[i]->uid == uid)
            {
                if (write(clients[i]->sockfd, message, strlen(message)) < 0)
                {
                    printf("ERROR: write to descriptor failed\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    char buffer[BUFFER_SZ];
    char command[BUFFER_SZ];
    int number; 
    char name[NAME_LEN];
    int leave_flag = 0;
    int flag = 0;

    client_t *cli = (client_t*)arg;

    // name
    if (recv(cli->sockfd, name, NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN -1)
    {
        printf("Enter the name correctly\n");
        leave_flag = 1;
    } else {
        strcpy(cli->name, name);
        sprintf(buffer, "> %s has joined\n", cli->name);
        printf("%s", buffer);

        bzero(buffer, BUFFER_SZ);
        sprintf(buffer, "ok");
        send_message(buffer, cli->uid);
    }

    bzero(buffer, BUFFER_SZ);

    while(leave_flag == 0)
    {
        command[0] = '\x00';
        number = 0;

        int receive = recv(cli->sockfd, buffer, BUFFER_SZ, 0);

        if (receive > 0)
        {
            if (strlen(buffer) > 0)
            {
                // send_message(buffer, cli->uid);
                trim_lf(buffer, strlen(buffer));
                printf("> client: '%s' has been send '%s' command\n", cli->name, buffer);
                sscanf(buffer, "%s %i", &command[0], &number);

                if (strcmp(command, "create") == 0)
                {
                    flag = 0;
                    
                    pthread_mutex_lock(&rooms_mutex);

                    for (int i = 0; i < MAX_ROOMS; i++) 
                    {
                        if (rooms[i])
                        {
                            if (rooms[i]->player1->uid == cli->uid)
                            {
                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] voce ja esta em uma sala\n");
                                send_message(buffer, cli->uid);
                                flag = 1;
                                break;
                            }

                            if (rooms[i]->player2 != 0)
                            {
                                if (rooms[i]->player2->uid == cli->uid)
                                {
                                    bzero(buffer, BUFFER_SZ);
                                    sprintf(buffer, "[SERVER] voce ja esta em uma sala\n");
                                    send_message(buffer, cli->uid);
                                    flag = 1;
                                    break;
                                }
                            }
                        }
                    } 

                    pthread_mutex_unlock(&rooms_mutex);

                    if (flag != 1) {
                        // clients settings
                        room_t *room = (room_t *)malloc(sizeof(room_t));
                        room->player1 = cli;
                        room->player2 = 0;
                        room->uid = roomUid;
                        strcpy(room->state, "waiting for secound player");

                        // add room to queue
                        queue_add_room(room);
                        bzero(buffer, BUFFER_SZ);
                        sprintf(buffer, "[SERVER] voce criou o novo room de numero %i\n", roomUid);
                        send_message(buffer, cli->uid);
                        roomUid++;
                    }
                }
                else if (strcmp(command, "join") == 0)
                {
                    int researched = 0;
                    int already = 0;
                    pthread_mutex_lock(&rooms_mutex);

                    for (int j = 0; j < MAX_ROOMS; j++) 
                    {
                        if (rooms[j])
                        {
                            if (rooms[j]->player1->uid == cli->uid) 
                            {
                                already = 1;

                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] voce ja esta no room de numero: %i\n", rooms[j]->uid);
                                send_message(buffer, cli->uid);
                                break;
                            }
                        }
                    }

                    pthread_mutex_unlock(&rooms_mutex);

                    if (already == 1)
                    {
                        continue;
                    }

                    pthread_mutex_lock(&rooms_mutex);

                    for (int i = 0; i < MAX_ROOMS; i++) 
                    {
                        if (rooms[i])
                        {
                            if (rooms[i]->uid == number)
                            {
                                researched = 1;

                                if (rooms[i]->player2 != 0)
                                {
                                    if (rooms[i]->player2->uid == cli->uid)
                                    {
                                        bzero(buffer, BUFFER_SZ);
                                        send_message("[SERVER] voce ja esta nesta sala\n", cli->uid);
                                        break;
                                    }

                                    bzero(buffer, BUFFER_SZ);
                                    sprintf(buffer, "[SERVER] room de numero: %i, ja esta cheio\n", rooms[i]->uid);
                                    send_message(buffer, cli->uid);
                                    break;
                                }

                                rooms[i]->player2 = cli;
                                strcpy(rooms[i]->state, "waiting start");
                                bzero(buffer, BUFFER_SZ);
                                printf("%s entrou no room de numero: %i\n", cli->name, number);
                                sprintf(buffer, "[SERVER] '%s' entrou em sua sala\n", cli->name);
                                send_message(buffer, rooms[i]->player1->uid);

                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] voce entrou no room de numero: %i\n", number);
                                send_message(buffer, cli->uid);
                                break;
                            }
                        }
                    }

                    pthread_mutex_unlock(&rooms_mutex);

                    if (researched == 0)
                    {
                        sprintf(buffer, "[SERVER] nao foi possivel encontrar a sala de numero %i\n", number);
                        send_message(buffer, cli->uid);
                    }
                }
                else if (strcmp(command, "list") == 0)
                {
                    pthread_mutex_lock(&rooms_mutex);

                    for (int i = 0; i < MAX_ROOMS; i++) 
                    {
                        if (rooms[i])
                        {
                            char *list = (char *)malloc(BUFFER_SZ*sizeof(char)); 

                            if (rooms[i]->player2 != 0)
                            {
                                sprintf(list, "%i)\n    room state: %s\n    player1: %s\n    player2: %s\n", rooms[i]->uid, rooms[i]->state, rooms[i]->player1->name, rooms[i]->player2->name);
                            }
                            else
                            {
                                sprintf(list, "%i)\n    room state: %s\n    player1: %s\n", rooms[i]->uid, rooms[i]->state, rooms[i]->player1->name);
                            }

                            send_message(list, cli->uid);
                            free(list);
                        }
                    } 

                    pthread_mutex_unlock(&rooms_mutex);
                }
                else if (strcmp(command, "leave") == 0)
                {
                    int remove_room = 0;
                    int room_number = 0;

                    pthread_mutex_lock(&rooms_mutex);

                    for (int i = 0; i < MAX_ROOMS; i++) 
                    {
                        if (rooms[i])
                        {
                            if (rooms[i]->player1->uid == cli->uid) 
                            {
                                if (rooms[i]->player2 != 0) {
                                    bzero(buffer, BUFFER_SZ);
                                    sprintf(buffer, "[SERVER] %s saio da sala, agora voce e o dono\n", rooms[i]->player1->name);
                                    send_message(buffer, rooms[i]->player2->uid);

                                    rooms[i]->player1 = rooms[i]->player2;
                                    rooms[i]->player2 = 0;
                                    strcpy(rooms[i]->state, "waiting for secound player");
                                }
                                else
                                {
                                    remove_room = 1;
                                    room_number = rooms[i]->uid;
                                }

                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] voce saio da sala %i\n", rooms[i]->uid);
                                send_message(buffer, cli->uid);
                                break;
                            }
                            else if (rooms[i]->player2->uid == cli->uid)
                            {
                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] %s saio de sua sala\n", rooms[i]->player1->name);
                                send_message(buffer, rooms[i]->player1->uid);

                                rooms[i]->player2 = 0;
                                strcpy(rooms[i]->state, "waiting for secound player");

                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] voce saio da sala %i\n", rooms[i]->uid);
                                send_message(buffer, cli->uid);
                                break;
                            }
                        }
                    }

                    pthread_mutex_unlock(&rooms_mutex);

                    if (remove_room == 1)
                    {
                        queue_remove_room(room_number);
                        roomUid--;
                    }
                }
                else if (strcmp(command, "startgame") == 0)
                {
                    int startgame = 0;
                    room_t *room_game;
                    
                    pthread_mutex_lock(&rooms_mutex);

                    for (int j = 0; j < MAX_ROOMS; j++) 
                    {
                        if (rooms[j])
                        {
                            if (rooms[j]->player1->uid == cli->uid) 
                            {
                                if (rooms[j]->player2 != 0)
                                {
                                    startgame = 1;
                                    room_game = rooms[j];
                                    break;
                                }

                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] e nescessario 2 players para poder iniciar a partida\n");
                                send_message(buffer, cli->uid);
                                break;
                            }
                            else if (rooms[j]->player2->uid == cli->uid)
                            {
                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "[SERVER] somente o dono da sala pode iniciar\n");
                                send_message(buffer, cli->uid);
                                break;
                            }
                        }
                    }

                    pthread_mutex_unlock(&rooms_mutex);

                    if (startgame == 1) {
                        room_game->game = (game_t *)malloc(sizeof(game_t));  
                        room_game->game->estadoDeJogo = 1;
                        room_game->game->rodada = 0;
                        room_game->game->turnoDoJogador = room_game->player1->uid;

                        for (int linha = 0; linha < 3; linha++)
                        {
                            for (int coluna = 0; coluna < 3; coluna++)
                            {
                                room_game->game->tabuleiro[linha][coluna] = '-';
                            }
                        }

                        bzero(buffer, BUFFER_SZ);
                        sprintf(buffer, "start game");
                        send_message(buffer, room_game->player1->uid);

                        bzero(buffer, BUFFER_SZ);
                        sprintf(buffer, "%s", room_game->player2->name);
                        send_message(buffer, room_game->player1->uid);

                        bzero(buffer, BUFFER_SZ);
                        sprintf(buffer, "start game 2");
                        send_message(buffer, room_game->player2->uid);

                        bzero(buffer, BUFFER_SZ);
                        sprintf(buffer, "%s", room_game->player1->name);
                        send_message(buffer, room_game->player2->uid);

                        sleep(1);

                        bzero(buffer, BUFFER_SZ);
                        sprintf(buffer, "vez 1");
                        send_message(buffer, room_game->player1->uid);

                        bzero(buffer, BUFFER_SZ);
                        sprintf(buffer, "vez 2");
                        send_message(buffer, room_game->player2->uid);
                    }
                }
                else if (strcmp(command, "play") == 0)
                {
                    pthread_mutex_lock(&rooms_mutex);

                    for (int j = 0; j < MAX_ROOMS; j++) 
                    {
                        if (rooms[j])
                        {
                            if (rooms[j]->player1->uid == cli->uid || rooms[j]->player2->uid == cli->uid) 
                            {
                                if (rooms[j]->game->estadoDeJogo == 0)
                                {
                                    bzero(buffer, BUFFER_SZ);
                                    sprintf(buffer, "[SERVER] esta partida ja acabou\n");
                                    send_message(buffer, cli->uid);
                                    break;
                                }

                                if (rooms[j]->game->turnoDoJogador != cli->uid)
                                {
                                    bzero(buffer, BUFFER_SZ);
                                    sprintf(buffer, "[SERVER] ainda nao e sua vez\n");
                                    send_message(buffer, cli->uid);
                                    break;
                                }

                                setbuf(stdin, 0);
                                bzero(buffer, BUFFER_SZ);
                                sprintf(buffer, "%i", number);
                                send_message(buffer, rooms[j]->player2->uid);

                                int linhaJogada = posicoes[number - 1][0];
                                int colunaJogada = posicoes[number - 1][1];
                                if (rooms[j]->game->turnoDoJogador == rooms[j]->player1->uid)
                                {
                                    rooms[j]->game->tabuleiro[linhaJogada][colunaJogada] = 'X';
                                    rooms[j]->game->turnoDoJogador = rooms[j]->player2->uid;
                                } 
                                else if (rooms[j]->game->turnoDoJogador == rooms[j]->player2->uid)
                                {
                                    rooms[j]->game->tabuleiro[linhaJogada][colunaJogada] = 'O';
                                    rooms[j]->game->turnoDoJogador = rooms[j]->player1->uid;
                                }

                                for (int iterator = 0; iterator < 3; iterator++)
                                {
                                    if (
                                        (
                                            (rooms[j]->game->tabuleiro[iterator][0] == rooms[j]->game->tabuleiro[iterator][1]) && (rooms[j]->game->tabuleiro[iterator][1] == rooms[j]->game->tabuleiro[iterator][2]) && rooms[j]->game->tabuleiro[iterator][0] != '-'
                                        )
                                            ||
                                        (
                                            (rooms[j]->game->tabuleiro[0][iterator] == rooms[j]->game->tabuleiro[1][iterator]) && (rooms[j]->game->tabuleiro[1][iterator] == rooms[j]->game->tabuleiro[2][iterator]) && rooms[j]->game->tabuleiro[0][iterator] != '-'
                                        )
                                    )
                                    {
                                        rooms[j]->game->estadoDeJogo = 0;
                                    }
                                }

                                if (
                                    (
                                        (rooms[j]->game->tabuleiro[0][0] == rooms[j]->game->tabuleiro[1][1]) && (rooms[j]->game->tabuleiro[1][1] == rooms[j]->game->tabuleiro[2][2]) && rooms[j]->game->tabuleiro[0][0] != '-'
                                    )
                                        ||
                                    (
                                        (rooms[j]->game->tabuleiro[0][2] == rooms[j]->game->tabuleiro[1][1]) && (rooms[j]->game->tabuleiro[1][1] == rooms[j]->game->tabuleiro[2][0]) && rooms[j]->game->tabuleiro[0][2] != '-'
                                    )
                                )
                                {
                                    rooms[j]->game->estadoDeJogo = 0;
                                }

                                sleep(1);

                                if (rooms[j]->game->turnoDoJogador == rooms[j]->player1->uid)
                                {
                                    setbuf(stdin, 0);
                                    bzero(buffer, BUFFER_SZ);

                                    sprintf(buffer, "vez 1");
                                    send_message(buffer, rooms[j]->player1->uid);

                                    sprintf(buffer, "vez 2");
                                    send_message(buffer, rooms[j]->player2->uid);
                                }
                                else if (rooms[j]->game->turnoDoJogador == rooms[j]->player2->uid)
                                {
                                    setbuf(stdin, 0);                  
                                    bzero(buffer, BUFFER_SZ);

                                    sprintf(buffer, "vez 2");
                                    send_message(buffer, rooms[j]->player1->uid);

                                    sprintf(buffer, "vez 1");
                                    send_message(buffer, rooms[j]->player2->uid);
                                }

                                rooms[j]->game->rodada++;
                            }
                            break;
                        }
                    }

                    pthread_mutex_unlock(&rooms_mutex);
                }
            }
        }    
        else if (receive == 0 || strcmp(buffer, "exit") == 0)
        {
            sprintf(buffer, "%s has left\n", cli->name);
            printf("%s", buffer);
            // send_message(buffer, cli->uid);
            leave_flag = 1;
        }
        else
        {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        bzero(buffer, BUFFER_SZ);
    }

    bzero(buffer, BUFFER_SZ);
    sprintf(buffer, "bye");
    send_message(buffer, cli->uid);

    close(cli->sockfd);
    queue_remove_client(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // Socket settings
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // Signals
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option)) < 0) {
        printf("ERROR: setsockopt\n");
        return EXIT_FAILURE;
    }

    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Error bind\n");
        return EXIT_FAILURE;
    }

    // listen
    if (listen(listenfd, 10) < 0) {
        printf("ERROR: listen\n");
        return EXIT_FAILURE;
    }

    flashScreen();

    printf("############################################");
    printf("\n# Tic-Tac-Toe Server running on port: %i #", port);
    printf("\n############################################\n\n");

    while(1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // check dor max clients
        if ((cli_count + 1) == MAX_CLIENTS) {
            printf("Maximun of clients are connected, Connection rejected");
            close(connfd);
            continue;
        }

        // clients settings
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        // add client to queue
        queue_add_client(cli);
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        // reduce CPU usage
        sleep(1);
    }
    
    return EXIT_SUCCESS;
}
