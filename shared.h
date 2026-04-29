#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>

#define PORT 8081
#define BUFFER_SIZE 256
#define MAX_PARTITE 10 
#define MAX_CLIENTS 100

typedef struct {
    int32_t tipo_messaggio; 
    // ... 1-20 (stessi di prima) ...
    // 21 = Messaggio Chat tra giocatori
    
    int32_t mossa_x;
    int32_t mossa_y;
    int32_t id_partita; 
    char payload[BUFFER_SIZE];
} PacchettoRete;

#endif