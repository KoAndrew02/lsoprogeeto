#ifndef SHARED_H
#define SHARED_H

#define PORT 8080
#define BUFFER_SIZE 1024

// Questa è la "scatola" che viaggerà sulla rete
typedef struct {
    int tipo_messaggio; // Es: 0 = "Voglio giocare", 1 = "Mossa", 2 = "Aggiornamento stato"
    int mossa_x;        // Coordinata X del Tris
    int mossa_y;        // Coordinata Y del Tris
    char payload[BUFFER_SIZE]; // Testo generico (es. "Sei in attesa", "Hai vinto!")
} PacchettoRete;

#endif