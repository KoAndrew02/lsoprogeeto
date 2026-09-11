// Inclusioni portabili per Windows e Linux
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netdb.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../shared/shared.h"

int main(int argc, char *argv[]) {
    int client_socket;
    struct sockaddr_in server_addr;
    struct hostent *server;
    
    // Impostiamo il nome del server di default a quello usato in Docker Compose
    char *server_name = "127.0.0.1";

    // Opzionale: se avvii il client da terminale con un parametro (es. ./client 127.0.0.1), usa quello
    if (argc > 1) {
        server_name = argv[1];
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "Errore nell'inizializzazione di Winsock.\n");
        exit(1);
    }
#endif

    // 1. Creazione del socket
#ifdef _WIN32
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (client_socket < 0) {
        perror("Errore nella creazione del socket");
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    // 2. Risoluzione del nome (Traduce "server-tris" nel suo indirizzo IP interno a Docker)
    server = gethostbyname(server_name);
    if (server == NULL) {
        fprintf(stderr, "Errore: impossibile trovare il server '%s'\n", server_name);
#ifdef _WIN32
        closesocket(client_socket);
        WSACleanup();
#else
        close(client_socket);
#endif
        exit(1);
    }

    // 3. Configurazione Indirizzo Server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    // 4. Connessione
    if (
#ifdef _WIN32
        connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR
#else
        connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0
#endif
    ) {
        perror("Errore nella connessione al server");
#ifdef _WIN32
        closesocket(client_socket);
        WSACleanup();
#else
        close(client_socket);
#endif
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
    int send_result;
#ifdef _WIN32
    send_result = send(client_socket, (const char*)&pacchetto_da_inviare, sizeof(PacchettoRete), 0);
#else
    send_result = send(client_socket, &pacchetto_da_inviare, sizeof(PacchettoRete), 0);
#endif
    if (send_result < 0) {
        perror("Errore nell'invio dei dati");
#ifdef _WIN32
        closesocket(client_socket);
        WSACleanup();
#else
        close(client_socket);
#endif
        return 1;
    }
    printf("Messaggio di presentazione inviato.\n");

    // 7. Attesa della primissima risposta dal Server
    PacchettoRete risposta;
    int recv_result;
#ifdef _WIN32
    recv_result = recv(client_socket, (char*)&risposta, sizeof(PacchettoRete), 0);
#else
    recv_result = recv(client_socket, &risposta, sizeof(PacchettoRete), 0);
#endif
    if (recv_result > 0) {
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
        if (fgets(input_utente, sizeof(input_utente), stdin) == NULL) {
            break;
        }
        input_utente[strcspn(input_utente, "\n")] = 0;
        if (strcmp(input_utente, "esci") == 0) {
            break;
        }
        PacchettoRete nuova_mossa;
        nuova_mossa.tipo_messaggio = 1;
        nuova_mossa.mossa_x = -1;
        nuova_mossa.mossa_y = -1;
        strcpy(nuova_mossa.payload, input_utente);
#ifdef _WIN32
        send_result = send(client_socket, (const char*)&nuova_mossa, sizeof(PacchettoRete), 0);
#else
        send_result = send(client_socket, &nuova_mossa, sizeof(PacchettoRete), 0);
#endif
        if (send_result < 0) {
            perror("Errore di invio");
            break;
        }
        PacchettoRete risposta_server;
#ifdef _WIN32
        recv_result = recv(client_socket, (char*)&risposta_server, sizeof(PacchettoRete), 0);
#else
        recv_result = recv(client_socket, &risposta_server, sizeof(PacchettoRete), 0);
#endif
        if (recv_result > 0) {
            printf("[Server]: %s\n", risposta_server.payload);
        } else if (recv_result == 0) {
            printf("\nIl Server ha chiuso la connessione.\n");
            break;
        } else {
            perror("Errore nella ricezione");
            break;
        }
    }

    // Chiusura pulita quando si esce dal while
#ifdef _WIN32
    closesocket(client_socket);
    WSACleanup();
#else
    close(client_socket);
#endif
    printf("Ti sei disconnesso.\n");
    return 0;
}

