#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "shared.h"

// Struttura per gestire una partita tra due giocatori
typedef struct {
    int giocatore_x;
    int giocatore_o;
    int turno_di_x; // 1 se tocca a X, 0 se tocca a O
} Partita;

Partita partita_corrente = {-1, -1, 1};
pthread_mutex_t mutex_partita = PTHREAD_MUTEX_INITIALIZER;

void invia_pacchetto(int socket, int tipo, char* msg, int x, int y) {
    PacchettoRete p;
    p.tipo_messaggio = tipo;
    p.mossa_x = x;
    p.mossa_y = y;
    strncpy(p.payload, msg, BUFFER_SIZE);
    send(socket, &p, sizeof(PacchettoRete), 0);
}

void *gestisci_client(void *arg) {
    int mio_socket = *(int*)arg;
    free(arg);
    int avversario_socket = -1;
    char mio_simbolo;

    // 1. ASSEGNAZIONE GIOCATORE
    pthread_mutex_lock(&mutex_partita);
    if (partita_corrente.giocatore_x == -1) {
        partita_corrente.giocatore_x = mio_socket;
        mio_simbolo = 'X';
        printf("Giocatore X connesso. In attesa di avversario...\n");
        invia_pacchetto(mio_socket, 2, "Sei il Giocatore X. In attesa di un avversario...", -1, -1);
    } else if (partita_corrente.giocatore_o == -1) {
        partita_corrente.giocatore_o = mio_socket;
        mio_simbolo = 'O';
        avversario_socket = partita_corrente.giocatore_x;
        printf("Giocatore O connesso. Partita iniziata!\n");
        
        // Avvisa entrambi che la partita inizia
        invia_pacchetto(avversario_socket, 2, "Avversario trovato! Tu sei X, tocca a te.", -1, -1);
        invia_pacchetto(mio_socket, 2, "Connesso! Tu sei O, attendi il turno di X.", -1, -1);
    }
    pthread_mutex_unlock(&mutex_partita);

    // 2. LOOP DI GIOCO
    PacchettoRete p;
    while (recv(mio_socket, &p, sizeof(PacchettoRete), 0) > 0) {
        // Cerchiamo l'avversario se non lo abbiamo ancora (nel caso fossimo X)
        if (avversario_socket == -1) {
            if (mio_simbolo == 'X') avversario_socket = partita_corrente.giocatore_o;
            else avversario_socket = partita_corrente.giocatore_x;
        }

        if (avversario_socket != -1) {
            printf("Mossa da %c: [%d,%d]. Inoltro all'avversario.\n", mio_simbolo, p.mossa_x, p.mossa_y);
            
            // Prepariamo il messaggio per l'avversario
            char msg[BUFFER_SIZE];
            sprintf(msg, "L'avversario ha segnato in [%d,%d]. Tocca a te!", p.mossa_x, p.mossa_y);
            
            // Inoltriamo la mossa all'altro client
            invia_pacchetto(avversario_socket, 1, msg, p.mossa_x, p.mossa_y);
        } else {
            invia_pacchetto(mio_socket, 2, "Ancora nessun avversario. Pazienta...", -1, -1);
        }
    }

    printf("Giocatore %c disconnesso.\n", mio_simbolo);
    close(mio_socket);
    return NULL;
}

int main() {
    setbuf(stdout, NULL);
    int server_socket;
    struct sockaddr_in server_addr;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);
    printf("Server Tris v2.0 (Arbitro) in ascolto...\n");

    while (1) {
        struct sockaddr_in c_addr;
        socklen_t size = sizeof(c_addr);
        int client_s = accept(server_socket, (struct sockaddr*)&c_addr, &size);
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_s;
        pthread_t t;
        pthread_create(&t, NULL, gestisci_client, (void*)new_sock);
        pthread_detach(t);
    }
    return 0;
}