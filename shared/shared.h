#ifndef SHARED_H // Inizia la guardia che evita inclusioni multiple di questo file header
#define SHARED_H // Definisce la macro che identifica l'header come gia incluso

#include <stdint.h> // Include i tipi interi a dimensione garantita, come int32_t

#define PORT 8081 // Porta di rete utilizzata dal server
#define BUFFER_SIZE 256 // Dimensione massima dell'array payload e dei dati testuali
#define MAX_PARTITE 10 // Numero massimo di partite gestite dal sistema
#define MAX_CLIENTS 100 // Numero massimo di client gestiti dal sistema

typedef struct { // Dichiara una struttura contenente i dati di un pacchetto di rete
    int32_t tipo_messaggio; // Identifica il tipo di messaggio inviato in rete
    // ... 1-20 (stessi di prima) ... // Mantiene i tipi di messaggio definiti nelle versioni precedenti
    // 21 = Messaggio Chat tra giocatori // Il valore 21 identifica una chat tra due giocatori
    
    int32_t mossa_x; // Coordina la posizione orizzontale della mossa
    int32_t mossa_y; // Coordina la posizione verticale della mossa
    int32_t id_partita; // Identifica la partita a cui appartiene il pacchetto
    char payload[BUFFER_SIZE]; // Contiene dati testuali aggiuntivi, come il testo della chat
} PacchettoRete; // Assegna alla struttura il nome PacchettoRete

#endif // Chiude la guardia di inclusione iniziata da #ifndef SHARED_H