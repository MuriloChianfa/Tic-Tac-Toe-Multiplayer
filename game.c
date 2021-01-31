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

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define NAME_LEN 32

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LEN];
int jogador = 1;

pthread_t lobby_thread;
pthread_t recv_msg_thread;
pthread_t multiplayer_game;

char *ip = "127.0.0.1";
int port = 2050;

void catch_ctrl_c_and_exit()
{
    flag = 1;
}

void showPositions()
{
    printf("\nMapa de posicoes:");
    printf("\n 7 | 8 | 9");
    printf("\n 4 | 5 | 6");
    printf("\n 1 | 2 | 3\n");
}

void showBoard(char tabuleiro[3][3], char *errorMessage)
{
    int linha;

    flashScreen();

    if (*errorMessage != '\x00')
    {
        printf("atencao: %s!\n\n", errorMessage);
        *errorMessage = '\x00';
    }

    printf("#############\n");

    for (linha = 0; linha < 3; linha++)
    {
        printf("# %c | %c | %c #", tabuleiro[linha][0], tabuleiro[linha][1], tabuleiro[linha][2]);

        printf("\n");
    }

    printf("#############\n");

    showPositions();
}

void createBoard(char tabuleiro[3][3])
{
    int linha, coluna;

    for (linha = 0; linha < 3; linha++)
    {
        for (coluna = 0; coluna < 3; coluna++)
        {
            tabuleiro[linha][coluna] = '-';
        }
    }
}


void menu();

void *lobby(void *arg);

void recv_msg_handler();

void *multiplayerGame(void *arg)
{
    int turnoDoJogador;
    turnoDoJogador = jogador;
    char nomeJogador1[32];
    char nomeJogador2[32];

    strcpy(nomeJogador1, name);

    char tabuleiro[3][3];
    int iterator;
    int linhaJogada, colunaJogada;
    int posicaoJogada;
    int rodada = 0;
    int estadoDeJogo = 1;
    int valid_play = 0;
    int played;
    int numberPlayed;
    int posicoes[9][2] = {{2, 0}, {2, 1}, {2, 2}, {1, 0}, {1, 1}, {1, 2}, {0, 0}, {0, 1}, {0, 2}};

    char errorMessage[255] = {'\x00'};
    char *nomeJogadorAtual;
    char message[BUFFER_SZ] = {};

    int receive = recv(sockfd, message, BUFFER_SZ, 0);

    if (receive > 0) {
        setbuf(stdin, 0);
        trim_lf(message, strlen(message));
        sscanf(message, "%s", &nomeJogador2[0]);

        setbuf(stdout, 0);
        setbuf(stdin, 0);

        bzero(message, BUFFER_SZ);

        createBoard(tabuleiro);
        rodada = 0;

        while (rodada < 9 && estadoDeJogo == 1)
        {
            if (turnoDoJogador == 1)
            {
                nomeJogadorAtual = (char *)&nomeJogador1;
            } 
            else
            {
                nomeJogadorAtual = (char *)&nomeJogador2;
            }

            showBoard(tabuleiro, (char *)&errorMessage);

            printf("\nRodada: %d", rodada);
            printf("\nJogador: %s\n", nomeJogadorAtual);

            while (valid_play == 0)
            {
                bzero(message, BUFFER_SZ);

                int receive = recv(sockfd, message, BUFFER_SZ, 0);

                if (receive > 0) {
                    valid_play = 1;

                    setbuf(stdin, 0);
                    setbuf(stdout, 0);

                    if (strcmp(message, "vez1\n") == 0)
                    {
                        printf("Digite uma posicao: ");
                        scanf("%d", &posicaoJogada);


                        linhaJogada = posicoes[posicaoJogada - 1][0];
                        colunaJogada = posicoes[posicaoJogada - 1][1];


                        if (valid_play == 1)
                        {
                            sprintf(message, "play %i\n", posicaoJogada);
                            send(sockfd, message, strlen(message), 0);
                            bzero(message, BUFFER_SZ);
                        }
                        
                    }
                    else if (strcmp(message, "vez2\n") == 0)
                    {
                        printf("O outro jogador esta jogando...\n");

                        played = 0;

                        while (played == 0)
                        {
                            int receive = recv(sockfd, message, BUFFER_SZ, 0);

                            if (receive > 0) {
                                sscanf(message, "%i", &numberPlayed);

                                linhaJogada = posicoes[numberPlayed - 1][0];
                                colunaJogada = posicoes[numberPlayed - 1][1];

                                played = 1;
                            }
                        }

                        valid_play = 1;
                    }
                }
                else
                {
                    valid_play = 0;
                }
            }

            if (turnoDoJogador == 1)
            {
                tabuleiro[linhaJogada][colunaJogada] = 'X';
                turnoDoJogador = 2;
            } 
            else
            {
                tabuleiro[linhaJogada][colunaJogada] = 'O';
                turnoDoJogador = 1;
            }

            for (iterator = 0; iterator < 3; iterator++)
            {
                if (
                    (
                        (tabuleiro[iterator][0] == tabuleiro[iterator][1]) && (tabuleiro[iterator][1] == tabuleiro[iterator][2]) && tabuleiro[iterator][0] != '-'
                    )
                        ||
                    (
                        (tabuleiro[0][iterator] == tabuleiro[1][iterator]) && (tabuleiro[1][iterator] == tabuleiro[2][iterator]) && tabuleiro[0][iterator] != '-'
                    )
                )
                {
                    estadoDeJogo = 0;
                }
            }

            if (
                (
                    (tabuleiro[0][0] == tabuleiro[1][1]) && (tabuleiro[1][1] == tabuleiro[2][2]) && tabuleiro[0][0] != '-'
                )
                    ||
                (
                    (tabuleiro[0][2] == tabuleiro[1][1]) && (tabuleiro[1][1] == tabuleiro[2][0]) && tabuleiro[0][2] != '-'
                )
            )
            {
                estadoDeJogo = 0;
            }

            rodada++;
            valid_play = 0;
            bzero(message, BUFFER_SZ);
        }

        bzero(message, BUFFER_SZ);

        int receive = recv(sockfd, message, BUFFER_SZ, 0);

        if (receive > 0) {
            setbuf(stdin, 0);
            setbuf(stdout, 0);

            showBoard(tabuleiro, (char *)&errorMessage);


            if (strcmp(message, "win1\n") == 0)
            {
                printf("\nO jogador '%s' venceu!", nomeJogadorAtual);
            }
            else if (strcmp(message, "win2\n") == 0)
            {
                printf("\nO jogador '%s' venceu!", nomeJogadorAtual);
            }

            printf("\nFim de jogo!\n");

            sleep(6);

            if (pthread_create(&lobby_thread, NULL, &lobby, NULL) != 0) {
                printf("ERROR: pthread\n");
                return NULL;
            }

            if (pthread_create(&recv_msg_thread, NULL, (void*)recv_msg_handler, NULL) != 0) {
                printf("ERROR: pthread\n");
                return NULL;
            }

            pthread_detach(pthread_self());
            pthread_cancel(multiplayer_game);

        }
        
    }

    return NULL;
}

void *lobby(void *arg)
{
    char buffer[BUFFER_SZ] = {};

    while(1)
    {
        str_overwrite_stdout();
        fgets(buffer, BUFFER_SZ, stdin);
        trim_lf(buffer, BUFFER_SZ);

        if (strcmp(buffer, "exit") == 0) {
            break;
        } else {
            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(buffer, BUFFER_SZ);
    }

    catch_ctrl_c_and_exit(2);

    return NULL;
}

void recv_msg_handler()
{
    char message[BUFFER_SZ] = {};

    flashScreen();

    while(1)
    {
        int receive = recv(sockfd, message, BUFFER_SZ, 0);

        if (receive > 0) {
            if (strcmp(message, "ok") == 0)
            {
                printf("Comandos:\n");
                printf("\t -list\t\t\t  List all tic-tac-toe rooms\n");
                printf("\t -create\t\t  Create one tic-tac-toe room\n");
                printf("\t -join {room number}\t  Join in one tic-tac-toe room\n");
                printf("\t -leave\t\t\t  Back of the one tic-tac-toe room\n");
                printf("\t -start\t\t\t  Starts one tic-tac-toe game\n\n");

                str_overwrite_stdout();
            }
            else if (strcmp(message, "start game\n") == 0)
            {
                pthread_cancel(lobby_thread);
                // pthread_kill(recv_msg_thread, SIGUSR1);   
                
                jogador = 1;
                if (pthread_create(&multiplayer_game, NULL, (void*)multiplayerGame, NULL) != 0) {
                    printf("ERROR: pthread\n");
                    exit(EXIT_FAILURE);
                }
                pthread_detach(pthread_self());
                pthread_cancel(recv_msg_thread);

                // pthread_kill(lobby_thread, SIGUSR1);   
            }
            else if (strcmp(message, "start game2\n") == 0)
            {
                pthread_cancel(lobby_thread);
                // pthread_kill(recv_msg_thread, SIGUSR1);   
                
                jogador = 2;
                if (pthread_create(&multiplayer_game, NULL, (void*)multiplayerGame, NULL) != 0) {
                    printf("ERROR: pthread\n");
                    exit(EXIT_FAILURE);
                }
                pthread_detach(pthread_self());
                pthread_cancel(recv_msg_thread);

                // pthread_kill(lobby_thread, SIGUSR1);   
            }
            else
            {
                printf("%s", message);
                str_overwrite_stdout();
            }
        } else if (receive == 0) {
            break;
        }

        bzero(message, BUFFER_SZ);
    }
}

void send_msg_handler()
{
    char buffer[BUFFER_SZ] = {};

    while(1)
    {
        str_overwrite_stdout();
        fgets(buffer, BUFFER_SZ, stdin);
        trim_lf(buffer, BUFFER_SZ);

        if (strcmp(buffer, "exit") == 0) {
            break;
        } else {
            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(buffer, BUFFER_SZ);
    }

    catch_ctrl_c_and_exit(2);
}

int conectGame()
{
    setbuf(stdin, 0);

    printf("Enter your name: ");
    fgets(name, BUFFER_SZ, stdin);
    trim_lf(name, BUFFER_SZ);

    // strcpy(name, "murilo");

    if (strlen(name) > NAME_LEN - 1 || strlen(name) < 2) {
        printf("Enter name corretly\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    
    //socket settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // connect to the server
    int err = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (err == -1) {
        printf("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    // send the name
    send(sockfd, name, NAME_LEN, 0);

    if (pthread_create(&lobby_thread, NULL, &lobby, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&recv_msg_thread, NULL, (void*)recv_msg_handler, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while(1)
    {
        if (flag) {
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);

    return 0;
}

void game()
{
    char tabuleiro[3][3];
    int iterator;
    int linhaJogada, colunaJogada;
    int posicaoJogada;
    int turnoDoJogador = 1;
    int rodada = 0;
    int estadoDeJogo = 1;
    int opcaoReinicio;
    int gaming = 1;

    char errorMessage[255] = {'\x00'};

    char nomeJogador1[32];
    char nomeJogador2[32];
    char *nomeJogadorAtual;

    setbuf(stdin, 0);

    printf("Nome do 1° jogador: ");
    fgets(nomeJogador1, 32, stdin);

    nomeJogador1[strlen(nomeJogador1) - 1] = '\x00';

    printf("Nome do 2° jogador: ");
    fgets(nomeJogador2, 32, stdin);

    nomeJogador2[strlen(nomeJogador2) - 1] = '\x00';

    while (gaming == 1)
    {
        createBoard(tabuleiro);
        rodada = 0;
        turnoDoJogador = 1;

        while (rodada < 9 && estadoDeJogo == 1)
        {
            showBoard(tabuleiro, (char *)&errorMessage);

            if (turnoDoJogador == 1)
            {
                nomeJogadorAtual = (char *)&nomeJogador1;
            } 
            else
            {
                nomeJogadorAtual = (char *)&nomeJogador2;
            }

            int posicoes[9][2] = {{2, 0}, {2, 1}, {2, 2}, {1, 0}, {1, 1}, {1, 2}, {0, 0}, {0, 1}, {0, 2}};

            printf("\nRodada: %d", rodada);
            printf("\nJogador: %s\n", nomeJogadorAtual);
            printf("Digite uma posicao: ");
            scanf("%d", &posicaoJogada);

            if (posicaoJogada < 1 || posicaoJogada > 9) {
                errorMessage[0] = '\x70';
                errorMessage[1] = '\x6F';
                errorMessage[2] = '\x73';
                errorMessage[3] = '\x69';
                errorMessage[4] = '\x63';
                errorMessage[5] = '\x61';
                errorMessage[6] = '\x6F';
                continue;
            }

            linhaJogada = posicoes[posicaoJogada - 1][0];
            colunaJogada = posicoes[posicaoJogada - 1][1];

            if (tabuleiro[linhaJogada][colunaJogada] != '-')
            {
                errorMessage[0] = '\x70';
                errorMessage[1] = '\x6F';
                errorMessage[2] = '\x73';
                errorMessage[3] = '\x69';
                errorMessage[4] = '\x63';
                errorMessage[5] = '\x61';
                errorMessage[6] = '\x6F';
                errorMessage[7] = '\x20';
                errorMessage[8] = '\x6A';
                errorMessage[9] = '\x61';
                errorMessage[10] = '\x20';
                errorMessage[11] = '\x65';
                errorMessage[12] = '\x6D';
                errorMessage[13] = '\x20';
                errorMessage[14] = '\x75';
                errorMessage[15] = '\x73';
                errorMessage[16] = '\x6F';
                continue;
            }

            if (turnoDoJogador == 1)
            {
                tabuleiro[linhaJogada][colunaJogada] = 'X';
                turnoDoJogador = 2;
            } 
            else
            {
                tabuleiro[linhaJogada][colunaJogada] = 'O';
                turnoDoJogador = 1;
            }

            for (iterator = 0; iterator < 3; iterator++)
            {
                if (
                    (
                        (tabuleiro[iterator][0] == tabuleiro[iterator][1]) && (tabuleiro[iterator][1] == tabuleiro[iterator][2]) && tabuleiro[iterator][0] != '-'
                    )
                        ||
                    (
                        (tabuleiro[0][iterator] == tabuleiro[1][iterator]) && (tabuleiro[1][iterator] == tabuleiro[2][iterator]) && tabuleiro[0][iterator] != '-'
                    )
                )
                {
                    estadoDeJogo = 0;
                }
            }

            if (
                (
                    (tabuleiro[0][0] == tabuleiro[1][1]) && (tabuleiro[1][1] == tabuleiro[2][2]) && tabuleiro[0][0] != '-'
                )
                    ||
                (
                    (tabuleiro[0][2] == tabuleiro[1][1]) && (tabuleiro[1][1] == tabuleiro[2][0]) && tabuleiro[0][2] != '-'
                )
            )
            {
                estadoDeJogo = 0;
            }

            rodada++;
        }

        showBoard(tabuleiro, (char *)&errorMessage);

        printf("\nO jogador '%s' venceu!", nomeJogadorAtual);

        printf("\nFim de jogo!\n");
        printf("\nDeseja reiniciar o jogo?");
        printf("\n1 - Sim");
        printf("\n2 - Nao");
        printf("\nEscolha uma opcao e tecle ENTER: ");

        scanf("%d", &opcaoReinicio);


        switch (opcaoReinicio)
        {
            case 1:
                estadoDeJogo = 1;
                break;
            case 2:
                menu();
                break;
        }
    }
}

void menu()
{
    int opcao = 0;

    flashScreen();

    signal(SIGINT, catch_ctrl_c_and_exit);

    while (opcao < 1 || opcao > 3)
    {
        printf("Bem vindo ao Jogo");
        printf("\n1 - Jogar Localmente");
        printf("\n2 - Jogar Online");
        printf("\n3 - Sobre");
        printf("\n4 - Sair");
        printf("\nEscolha uma opcao e tecle ENTER: ");

        scanf("%d", &opcao);

        switch (opcao)
        {
            case 1:
                flashScreen();
                opcao = 0;
                game();
                break;
            case 2:
                flashScreen();
                opcao = 0;
                exit(conectGame());
                break;
            case 3:
                flashScreen();
                opcao = 0;
                printf("Sobre o jogo!\n");
                break;
            case 4:
                flashScreen();
                opcao = 0;
                printf("Voce saio!\n");
                exit(0);
                break;
            default:
                flashScreen();
                opcao = 0;
                printf("opcao invalida!\n");
                continue;
                break;
        }
    }
}

int main(int argc, char **argv)
{
    menu();

    return 0;
}
