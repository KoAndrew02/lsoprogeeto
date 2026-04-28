#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>

#define PORT 8081
#define BUFFER_SIZE 256

typedef struct {
    int32_t tipo_messaggio; 
    // 1 = Mossa
    // 2 = Testo di stato
    // 3 = Hai Vinto
    // 4 = Hai Perso
    // 5 = Pareggio
    // 6 = Voglio Rigiocare (Client -> Server)
    // 8 = Pulisci Griglia (Server -> Client)
    int32_t mossa_x;
    int32_t mossa_y;
    char payload[BUFFER_SIZE];
} PacchettoRete;

#endif