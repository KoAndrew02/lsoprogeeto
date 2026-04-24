#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>   // Serve per gethostbyname()
#include "shared.h"

int main(int argc, char *argv[]) {
    int client_socket;
    struct sockaddr_in server_addr;
    struct hostent *server;
    
    // Impostiamo il nome del server di default a quello usato in Docker Compose
    char *server_name = "server-tris"; 

    // Opzionale: se avvii il client da terminale con un parametro (es. ./client 127.0.0.1), usa quello
    if (argc > 1) {
        server_name = argv[1];
    }

    // 1. Creazione del socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Errore nella creazione del socket");
        exit(1);
    }

    // 2. Risoluzione del nome (Traduce "server-tris" nel suo indirizzo IP interno a Docker)
    server = gethostbyname(server_name);
    if (server == NULL) {
        fprintf(stderr, "Errore: impossibile trovare il server '%s'\n", server_name);
        exit(1);
    }

    // 3. Configurazione Indirizzo Server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Copiamo l'indirizzo IP risolto da gethostbyname dentro la struttura del server
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    // 4. Connessione
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Errore nella connessione al server");
        exit(1);
    }
    printf("Connesso con successo al Server di Tris!\n");

    // 5. Preparazione del pacchetto di presentazione da inviare appena connessi
    PacchettoRete pacchetto_da_inviare;
    pacchetto_da_inviare.tipo_messaggio = 0; // Tipo: Connessione iniziale
    pacchetto_da_inviare.mossa_x = -1;       // Nessuna mossa per ora
    pacchetto_da_inviare.mossa_y = -1;
    strcpy(pacchetto_da_inviare.payload, "Ciao, vorrei entrare in una partita!");

    // 6. Invio dei dati di presentazione
    if (send(client_socket, &pacchetto_da_inviare, sizeof(PacchettoRete), 0) < 0) {
        perror("Errore nell'invio dei dati");
        close(client_socket);
        return 1;
    }
    printf("Messaggio di presentazione inviato.\n");

    // 7. Attesa della primissima risposta dal Server
    PacchettoRete risposta;
    if (recv(client_socket, &risposta, sizeof(PacchettoRete), 0) > 0) {
        printf("\n--- RISPOSTA DAL SERVER ---\n");
        printf("Tipo: %d\n", risposta.tipo_messaggio);
        printf("Messaggio: %s\n", risposta.payload);
        printf("---------------------------\n");
    }

    // ------------------------------------------
    // --- IL LOOP DI GIOCO INTERATTIVO ---
    // ------------------------------------------
    printf("\nSei nella lobby! Scrivi un messaggio da inviare al server (scrivi 'esci' per terminare):\n");
    
    char input_utente[256];
    
    while (1) {
        printf("Inserisci comando: ");
        // Leggiamo l'input dell'utente
        if (fgets(input_utente, sizeof(input_utente), stdin) == NULL) {
            break; // Se c'è un errore di lettura (es. Ctrl+D), usciamo
        }
        
        // Rimuoviamo l'accapo finale (\n) lasciato da fgets
        input_utente[strcspn(input_utente, "\n")] = 0; 
        
        // Se l'utente scrive "esci", interrompiamo il ciclo
        if (strcmp(input_utente, "esci") == 0) {
            break;
        }

        // Prepariamo un nuovo pacchetto con quello che ha scritto l'utente
        PacchettoRete nuova_mossa;
        nuova_mossa.tipo_messaggio = 1; // 1 = Messaggio standard
        nuova_mossa.mossa_x = -1;
        nuova_mossa.mossa_y = -1;
        strcpy(nuova_mossa.payload, input_utente);

        // Inviamo al server
        if (send(client_socket, &nuova_mossa, sizeof(PacchettoRete), 0) < 0) {
            perror("Errore di invio");
            break;
        }
        
        // Aspettiamo la risposta del server
        PacchettoRete risposta_server;
        int bytes_ricevuti = recv(client_socket, &risposta_server, sizeof(PacchettoRete), 0);
        
        if (bytes_ricevuti > 0) {
            printf("[Server]: %s\n", risposta_server.payload);
        } else if (bytes_ricevuti == 0) {
            printf("\nIl Server ha chiuso la connessione.\n");
            break;
        } else {
            perror("Errore nella ricezione");
            break;
        }
    }

    // Chiusura pulita quando si esce dal while
    close(client_socket);
    printf("Ti sei disconnesso.\n");
    return 0;
}