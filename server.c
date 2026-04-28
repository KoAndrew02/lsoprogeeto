#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "shared.h"

typedef struct {
    int proprietario; 
    int giocatore_stella;
    int giocatore_luna;
    char griglia[3][3];
    int mosse_fatte;
} Partita;

Partita partita_corrente = {-1, -1, -1, {{' ',' ',' '},{' ',' ',' '},{' ',' ',' '}}, 0};
pthread_mutex_t mutex_partita = PTHREAD_MUTEX_INITIALIZER;

void invia_pacchetto(int socket, int tipo, char* msg, int x, int y) {
    if (socket == -1) return;
    PacchettoRete p;
    memset(&p, 0, sizeof(PacchettoRete));
    p.tipo_messaggio = tipo;
    p.mossa_x = x;
    p.mossa_y = y;
    strncpy(p.payload, msg, BUFFER_SIZE - 1);
    send(socket, (char*)&p, sizeof(PacchettoRete), 0);
}

char controlla_vittoria() {
    char (*g)[3] = partita_corrente.griglia;
    for(int i=0; i<3; i++) {
        if((g[i][0] == 'S' || g[i][0] == 'L') && g[i][0] == g[i][1] && g[i][1] == g[i][2]) return g[i][0]; 
        if((g[0][i] == 'S' || g[0][i] == 'L') && g[0][i] == g[1][i] && g[1][i] == g[2][i]) return g[0][i]; 
    }
    if((g[0][0] == 'S' || g[0][0] == 'L') && g[0][0] == g[1][1] && g[1][1] == g[2][2]) return g[0][0]; 
    if((g[0][2] == 'S' || g[0][2] == 'L') && g[0][2] == g[1][1] && g[1][1] == g[2][0]) return g[0][2]; 
    return ' '; 
}

void resetta_griglia() {
    for(int r=0; r<3; r++) {
        for(int c=0; c<3; c++) {
            partita_corrente.griglia[r][c] = ' ';
        }
    }
    partita_corrente.mosse_fatte = 0;
}

void *gestisci_client(void *arg) {
    int mio_socket = *(int*)arg;
    free(arg);
    int avversario_socket = -1;
    char mio_simbolo = ' ';

    sleep(1); 

    pthread_mutex_lock(&mutex_partita);
    
    // --- IL FIX DEFINITIVO ---
    // Se non c'è nessuno in stanza, puliamo la memoria residua delle vecchie partite!
    if (partita_corrente.giocatore_stella == -1 && partita_corrente.giocatore_luna == -1) {
        resetta_griglia();
    }

    if (partita_corrente.giocatore_stella == -1) {
        partita_corrente.giocatore_stella = mio_socket;
        mio_simbolo = 'S';
        if (partita_corrente.proprietario == -1) partita_corrente.proprietario = mio_socket;
        invia_pacchetto(mio_socket, 2, "Sei Stella (★). In attesa di avversario...", -1, -1);
    } else if (partita_corrente.giocatore_luna == -1) {
        partita_corrente.giocatore_luna = mio_socket;
        mio_simbolo = 'L';
        avversario_socket = partita_corrente.giocatore_stella;
        invia_pacchetto(avversario_socket, 2, "Avversario trovato! Tu sei Stella (★), tocca a te.", -1, -1);
        invia_pacchetto(mio_socket, 2, "Connesso! Tu sei Luna (☾), attendi Stella.", -1, -1);
    }
    pthread_mutex_unlock(&mutex_partita);

    PacchettoRete p;
    memset(&p, 0, sizeof(PacchettoRete));
    
    while (recv(mio_socket, (char*)&p, sizeof(PacchettoRete), 0) > 0) {
        avversario_socket = (mio_simbolo == 'S') ? partita_corrente.giocatore_luna : partita_corrente.giocatore_stella;

        if (p.tipo_messaggio == 1 && avversario_socket != -1) {
            pthread_mutex_lock(&mutex_partita);
            partita_corrente.griglia[p.mossa_x][p.mossa_y] = mio_simbolo;
            partita_corrente.mosse_fatte++;
            
            char vincitore = controlla_vittoria();
            
            if (vincitore != ' ') {
                invia_pacchetto(mio_socket, 3, "Hai Vinto!", p.mossa_x, p.mossa_y); 
                invia_pacchetto(avversario_socket, 4, "Hai Perso!", p.mossa_x, p.mossa_y); 
            } else if (partita_corrente.mosse_fatte == 9) {
                invia_pacchetto(mio_socket, 5, "Pareggio!", p.mossa_x, p.mossa_y);
                invia_pacchetto(avversario_socket, 5, "Pareggio!", p.mossa_x, p.mossa_y);
            } else {
                invia_pacchetto(avversario_socket, 1, "Tocca a te!", p.mossa_x, p.mossa_y);
            }
            pthread_mutex_unlock(&mutex_partita);
        }
        else if (p.tipo_messaggio == 6) { // Richiesta Rivincita/Crea Partita
            pthread_mutex_lock(&mutex_partita);
            partita_corrente.proprietario = mio_socket;
            resetta_griglia();
            invia_pacchetto(mio_socket, 8, "Griglia pulita", -1, -1);
            invia_pacchetto(mio_socket, 2, "Sei il Proprietario. In attesa di uno sfidante...", -1, -1);
            pthread_mutex_unlock(&mutex_partita);
        }
        memset(&p, 0, sizeof(PacchettoRete));
    }

    // Gestione disconnessione e svuotamento stanza
    pthread_mutex_lock(&mutex_partita);
    if(partita_corrente.giocatore_stella == mio_socket) partita_corrente.giocatore_stella = -1;
    if(partita_corrente.giocatore_luna == mio_socket) partita_corrente.giocatore_luna = -1;
    if(partita_corrente.proprietario == mio_socket) partita_corrente.proprietario = -1;
    pthread_mutex_unlock(&mutex_partita);
    close(mio_socket);
    return NULL;
}

int main() {
    setbuf(stdout, NULL);
    resetta_griglia(); 
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);
    printf("Server Tris v5 (Pulizia Automatica Memoria) in ascolto...\n");

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