#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "shared.h"

typedef struct {
    int stato; int proprietario; int giocatore_stella; int giocatore_luna;
    int sfidante_in_attesa; char griglia[3][3]; int mosse_fatte;
} Partita;

Partita stanze[MAX_PARTITE];
int client_connessi[MAX_CLIENTS];
pthread_mutex_t mutex_server = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_clienti = PTHREAD_MUTEX_INITIALIZER;

void inizializza_server() {
    for(int i=0; i<MAX_PARTITE; i++) {
        stanze[i].stato = 0; stanze[i].proprietario = -1;
        stanze[i].giocatore_stella = -1; stanze[i].giocatore_luna = -1;
        stanze[i].sfidante_in_attesa = -1;
    }
    for(int i=0; i<MAX_CLIENTS; i++) client_connessi[i] = 0;
}

void invia_pacchetto(int socket, int tipo, char* msg, int x, int y, int id_partita) {
    if (socket <= 0) return;
    PacchettoRete p; memset(&p, 0, sizeof(PacchettoRete));
    p.tipo_messaggio = tipo; p.mossa_x = x; p.mossa_y = y; p.id_partita = id_partita;
    strncpy(p.payload, msg, BUFFER_SIZE - 1);
    send(socket, (char*)&p, sizeof(PacchettoRete), 0);
}

void broadcast_lobby(char *msg) {
    pthread_mutex_lock(&mutex_clienti);
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(client_connessi[i] > 0) invia_pacchetto(client_connessi[i], 20, msg, -1, -1, -1);
    }
    pthread_mutex_unlock(&mutex_clienti);
}

char controlla_vittoria(int id) {
    char (*g)[3] = stanze[id].griglia;
    for(int i=0; i<3; i++) {
        if((g[i][0] == 'S' || g[i][0] == 'L') && g[i][0] == g[i][1] && g[i][1] == g[i][2]) return g[i][0]; 
        if((g[0][i] == 'S' || g[0][i] == 'L') && g[0][i] == g[1][i] && g[1][i] == g[2][i]) return g[0][i]; 
    }
    if((g[0][0] == 'S' || g[0][0] == 'L') && g[0][0] == g[1][1] && g[1][1] == g[2][2]) return g[0][0]; 
    if((g[0][2] == 'S' || g[0][2] == 'L') && g[0][2] == g[1][1] && g[1][1] == g[2][0]) return g[0][2]; 
    return ' '; 
}

void resetta_griglia(int id) {
    for(int r=0; r<3; r++) {
        for(int c=0; c<3; c++) stanze[id].griglia[r][c] = ' ';
    }
    stanze[id].mosse_fatte = 0;
}

void abbandona_stanza(int socket, int *id_partita) {
    int id = *id_partita;
    if (id < 0 || id >= MAX_PARTITE) return;
    pthread_mutex_lock(&mutex_server);
    if(stanze[id].giocatore_stella == socket) stanze[id].giocatore_stella = -1;
    if(stanze[id].giocatore_luna == socket) stanze[id].giocatore_luna = -1;
    if(stanze[id].proprietario == socket) stanze[id].proprietario = -1;
    if(stanze[id].giocatore_stella == -1 && stanze[id].giocatore_luna == -1) stanze[id].stato = 0; 
    else {
        int rimasto = (stanze[id].giocatore_stella != -1) ? stanze[id].giocatore_stella : stanze[id].giocatore_luna;
        stanze[id].proprietario = rimasto; stanze[id].stato = 2; 
        invia_pacchetto(rimasto, 2, "L'avversario ha lasciato.", -1, -1, id);
    }
    *id_partita = -1; pthread_mutex_unlock(&mutex_server);
}

void *gestisci_client(void *arg) {
    int mio_socket = *(int*)arg;
    free(arg);
    int mia_partita = -1;
    char mio_simbolo = ' ';

    pthread_mutex_lock(&mutex_clienti);
    for(int i=0; i<MAX_CLIENTS; i++) { if(client_connessi[i] == 0) { client_connessi[i] = mio_socket; break; } }
    pthread_mutex_unlock(&mutex_clienti);

    PacchettoRete p;
    while (recv(mio_socket, (char*)&p, sizeof(PacchettoRete), 0) > 0) {
        pthread_mutex_lock(&mutex_server);
        if (mia_partita == -1) {
            for(int i = 0; i < MAX_PARTITE; i++) {
                if (stanze[i].giocatore_luna == mio_socket || stanze[i].giocatore_stella == mio_socket) {
                    mia_partita = i; mio_simbolo = (stanze[i].giocatore_stella == mio_socket) ? 'S' : 'L'; break;
                }
            }
        }
        pthread_mutex_unlock(&mutex_server);

        if (p.tipo_messaggio == 10) { // Crea
            pthread_mutex_lock(&mutex_server);
            for(int i=0; i<MAX_PARTITE; i++) {
                if(stanze[i].stato == 0) { 
                    stanze[i].stato = 2; stanze[i].proprietario = mio_socket; stanze[i].giocatore_stella = mio_socket;
                    mia_partita = i; mio_simbolo = 'S'; resetta_griglia(i);
                    char msg[BUFFER_SIZE]; sprintf(msg, "ID Stanza: %d | Sei Stella (★).", i);
                    invia_pacchetto(mio_socket, 14, msg, -1, -1, i); 
                    char bmsg[BUFFER_SIZE]; sprintf(bmsg, "[NUOVA CREAZIONE] Stanza %d libera!", i);
                    broadcast_lobby(bmsg); break;
                }
            }
            pthread_mutex_unlock(&mutex_server);
        }
        else if (p.tipo_messaggio == 11) { // Join
            pthread_mutex_lock(&mutex_server);
            int id = p.id_partita;
            if (id >= 0 && id < MAX_PARTITE && stanze[id].stato == 2) {
                stanze[id].sfidante_in_attesa = mio_socket;
                invia_pacchetto(stanze[id].proprietario, 12, "Qualcuno vuole sfidarti!", -1, -1, id);
            } else invia_pacchetto(mio_socket, 2, "Stanza non valida.", -1, -1, -1);
            pthread_mutex_unlock(&mutex_server);
        }
        else if (p.tipo_messaggio == 13) { // Risposta
            pthread_mutex_lock(&mutex_server);
            int id = mia_partita;
            if (id != -1 && stanze[id].sfidante_in_attesa != -1) {
                int sfidante = stanze[id].sfidante_in_attesa;
                if (p.mossa_x == 1) { 
                    stanze[id].giocatore_luna = sfidante; stanze[id].stato = 3; 
                    invia_pacchetto(sfidante, 14, "Entrato! Sei Luna (☾).", -1, -1, id); 
                    invia_pacchetto(mio_socket, 2, "Iniziata! Tocca a te (★).", -1, -1, id);
                } else invia_pacchetto(sfidante, 15, "Rifiutato.", -1, -1, -1);
                stanze[id].sfidante_in_attesa = -1;
            }
            pthread_mutex_unlock(&mutex_server);
        }
        else if (p.tipo_messaggio == 1 && mia_partita != -1) { // Mossa
            pthread_mutex_lock(&mutex_server);
            int id = mia_partita; int avv = (mio_simbolo == 'S') ? stanze[id].giocatore_luna : stanze[id].giocatore_stella;
            stanze[id].griglia[p.mossa_x][p.mossa_y] = mio_simbolo; stanze[id].mosse_fatte++;
            char v = controlla_vittoria(id);
            if (v != ' ' || stanze[id].mosse_fatte == 9) {
                stanze[id].stato = 4;
                if(v != ' ') { invia_pacchetto(mio_socket, 3, "Vinto!", p.mossa_x, p.mossa_y, id); invia_pacchetto(avv, 4, "Perso!", p.mossa_x, p.mossa_y, id); }
                else { invia_pacchetto(mio_socket, 5, "Pareggio!", p.mossa_x, p.mossa_y, id); invia_pacchetto(avv, 5, "Pareggio!", p.mossa_x, p.mossa_y, id); }
            } else invia_pacchetto(avv, 1, "Tocca a te!", p.mossa_x, p.mossa_y, id);
            pthread_mutex_unlock(&mutex_server);
        }
        // NUOVA LOGICA: INOLTRO CHAT
        else if (p.tipo_messaggio == 21 && mia_partita != -1) {
            pthread_mutex_lock(&mutex_server);
            int id = mia_partita;
            int avv = (mio_simbolo == 'S') ? stanze[id].giocatore_luna : stanze[id].giocatore_stella;
            if (avv != -1) invia_pacchetto(avv, 21, p.payload, -1, -1, id);
            pthread_mutex_unlock(&mutex_server);
        }
        else if (p.tipo_messaggio == 16) abbandona_stanza(mio_socket, &mia_partita);
        memset(&p, 0, sizeof(PacchettoRete));
    }
    abbandona_stanza(mio_socket, &mia_partita);
    pthread_mutex_lock(&mutex_clienti);
    for(int i=0; i<MAX_CLIENTS; i++) { if(client_connessi[i] == mio_socket) { client_connessi[i] = 0; break; } }
    pthread_mutex_unlock(&mutex_clienti);
    close(mio_socket); return NULL;
}

int main() {
    setbuf(stdout, NULL); signal(SIGPIPE, SIG_IGN); inizializza_server();
    int s = socket(AF_INET, SOCK_STREAM, 0); int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr; addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT); addr.sin_addr.s_addr = INADDR_ANY;
    bind(s, (struct sockaddr*)&addr, sizeof(addr)); listen(s, 10);
    printf("Server Tris v11 (Chat Attiva) in ascolto...\n");
    while (1) {
        struct sockaddr_in c_addr; socklen_t size = sizeof(c_addr);
        int client_s = accept(s, (struct sockaddr*)&c_addr, &size);
        int *new_sock = malloc(sizeof(int)); *new_sock = client_s;
        pthread_t t; pthread_create(&t, NULL, gestisci_client, (void*)new_sock); pthread_detach(t);
    }
    return 0;
}