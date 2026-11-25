// Seleziona le librerie di rete appropriate al sistema operativo.
#ifdef _WIN32 // Attiva questo ramo quando il programma viene compilato su Windows.
    #include <winsock2.h> // Importa le funzioni socket di Windows.
    #include <ws2tcpip.h> // Importa funzioni aggiuntive per gli indirizzi TCP/IP.
    #pragma comment(lib, "ws2_32.lib") // Chiede al compilatore MSVC di collegare la libreria Winsock.
#else // Attiva questo ramo sui sistemi diversi da Windows, ad esempio Linux.
    #include <unistd.h> // Fornisce close e altre funzioni POSIX.
    #include <arpa/inet.h> // Fornisce conversioni e strutture per gli indirizzi Internet.
    #include <netdb.h> // Fornisce le funzioni per risolvere i nomi degli host.
#endif // Termina la selezione delle librerie specifiche del sistema operativo.
#include <stdio.h> // Fornisce printf, fprintf e perror per stampare testo ed errori.
#include <stdlib.h> // Fornisce exit per terminare il programma in caso di errore.
#include <string.h> // Fornisce funzioni per copiare e manipolare stringhe e memoria.
#include "../shared/shared.h" // Importa la struttura PacchettoRete e la costante PORT condivise col server.

int main(int argc, char *argv[]) {
    int client_socket; // Conserverà il descrittore del socket usato per la connessione.
    struct sockaddr_in server_addr; // Conserverà l'indirizzo IPv4 e la porta del server.
    struct hostent *server; // Conserverà il risultato della risoluzione del nome del server.
    
    // Impostiamo il nome del server di default a quello usato in Docker Compose
    char *server_name = "127.0.0.1"; // Usa il computer locale come server se non viene specificato un parametro.

    // Opzionale: se avvii il client da terminale con un parametro (es. ./client 127.0.0.1), usa quello
    if (argc > 1) {
        server_name = argv[1]; // Sostituisce l'indirizzo predefinito con il primo argomento della riga di comando.
    }

#ifdef _WIN32 // Le operazioni seguenti servono solo nella compilazione per Windows.
    WSADATA wsaData; // Dichiara una struttura che Winsock riempie con i propri dati iniziali.
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) { // Inizializza Winsock nella versione 2.2 e verifica l'esito.
        fprintf(stderr, "Errore nell'inizializzazione di Winsock.\n"); // Scrive il messaggio di errore sul flusso standard degli errori.
        exit(1); // Termina il programma indicando che si è verificato un errore.
    }
#endif // Termina il blocco di inizializzazione specifico di Windows.

    // 1. Creazione del socket
#ifdef _WIN32 // Se il sistema è Windows, specifica esplicitamente il protocollo TCP.
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Crea un socket IPv4 orientato alla connessione TCP.
#else // Su Linux il protocollo TCP viene scelto automaticamente usando 0.
    client_socket = socket(AF_INET, SOCK_STREAM, 0); // Crea un socket IPv4 orientato alla connessione.
#endif // Termina la scelta della chiamata socket in base al sistema.
    if (client_socket < 0) { // Verifica se la creazione del socket è fallita.
        perror("Errore nella creazione del socket"); // Stampa il dettaglio dell'ultimo errore di sistema.
#ifdef _WIN32 // In caso di errore su Windows occorre anche chiudere Winsock.
        WSACleanup(); // Rilascia le risorse inizializzate da WSAStartup.
#endif // Termina la pulizia specifica di Windows.
        exit(1); // Interrompe il programma perché non può comunicare senza socket.
    }

    // 2. Risoluzione del nome (Traduce "server-tris" nel suo indirizzo IP interno a Docker)
    server = gethostbyname(server_name); // Traduce il nome o l'indirizzo del server in un indirizzo IPv4.
    if (server == NULL) { // Verifica se la risoluzione del nome non è riuscita.
        fprintf(stderr, "Errore: impossibile trovare il server '%s'\n", server_name); // Indica quale server non è stato trovato.
#ifdef _WIN32 // Su Windows i socket si chiudono con closesocket.
        closesocket(client_socket); // Chiude il socket creato in precedenza.
        WSACleanup(); // Rilascia le risorse di Winsock.
#else // Sugli altri sistemi si usa la funzione POSIX close.
        close(client_socket); // Chiude il socket creato in precedenza.
#endif // Termina la chiusura del socket specifica del sistema.
        exit(1); // Termina perché non è possibile individuare il server.
    }

    // 3. Configurazione Indirizzo Server
    memset(&server_addr, 0, sizeof(server_addr)); // Azzera la struttura prima di impostare i campi necessari.
    server_addr.sin_family = AF_INET; // Specifica che il server usa indirizzi IPv4.
    server_addr.sin_port = htons(PORT); // Imposta la porta condivisa convertendola nel formato di rete.
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length); // Copia nella struttura il primo indirizzo IP trovato.

    // 4. Connessione
    if (
#ifdef _WIN32 // Winsock segnala un errore restituendo il valore SOCKET_ERROR.
        connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR // Tenta la connessione al server e controlla l'esito secondo Winsock.
#else // POSIX segnala un errore restituendo un valore negativo.
        connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 // Tenta la connessione al server e controlla l'esito secondo POSIX.
#endif // Termina la selezione della condizione d'errore specifica del sistema.
    ) { // Entra qui se la connessione non è riuscita.
        perror("Errore nella connessione al server"); // Stampa il dettaglio dell'errore di connessione.
#ifdef _WIN32 // Su Windows la chiusura del socket usa l'API Winsock.
        closesocket(client_socket); // Chiude il socket.
        WSACleanup(); // Rilascia le risorse di Winsock.
#else // Su Linux e sugli altri sistemi POSIX si usa close.
        close(client_socket); // Chiude il socket.
#endif // Termina la chiusura specifica del sistema.
        exit(1); // Termina perché non è possibile proseguire senza connessione.
    }
    printf("Connesso con successo al Server di Tris!\n"); // Conferma all'utente che la connessione è attiva.

    // 5. Preparazione del pacchetto di presentazione da inviare appena connessi
    PacchettoRete pacchetto_da_inviare; // Dichiara la struttura che conterrà il primo messaggio per il server.
    pacchetto_da_inviare.tipo_messaggio = 0; // Imposta il tipo 0, usato per la presentazione iniziale.
    pacchetto_da_inviare.mossa_x = -1; // Indica che questo pacchetto non contiene una coordinata di mossa.
    pacchetto_da_inviare.mossa_y = -1; // Indica che questo pacchetto non contiene una seconda coordinata di mossa.
    strcpy(pacchetto_da_inviare.payload, "Ciao, vorrei entrare in una partita!"); // Inserisce il testo di presentazione nel payload.

    // 6. Invio dei dati di presentazione
    int send_result; // Conserverà il numero di byte inviati o l'indicatore d'errore.
#ifdef _WIN32 // La funzione send di Windows richiede un puntatore a char.
    send_result = send(client_socket, (const char*)&pacchetto_da_inviare, sizeof(PacchettoRete), 0); // Invia il pacchetto iniziale usando Winsock.
#else // La funzione send POSIX accetta il puntatore al buffer come void*.
    send_result = send(client_socket, &pacchetto_da_inviare, sizeof(PacchettoRete), 0); // Invia il pacchetto iniziale usando l'API POSIX.
#endif // Termina la selezione della chiamata send specifica del sistema.
    if (send_result < 0) { // Controlla se l'invio del pacchetto è fallito.
        perror("Errore nell'invio dei dati"); // Stampa il dettaglio dell'errore di invio.
#ifdef _WIN32 // Chiude il socket secondo l'API di Windows.
        closesocket(client_socket); // Chiude la connessione.
        WSACleanup(); // Rilascia le risorse di Winsock.
#else // Chiude il socket secondo l'API POSIX.
        close(client_socket); // Chiude la connessione.
#endif // Termina la pulizia specifica del sistema.
        return 1; // Termina main indicando un errore.
    }
    printf("Messaggio di presentazione inviato.\n"); // Conferma che il messaggio iniziale è stato inviato.

    // 7. Attesa della primissima risposta dal Server
    PacchettoRete risposta; // Dichiara la struttura che conterrà il primo pacchetto ricevuto.
    int recv_result; // Conserverà il numero di byte ricevuti o l'indicatore d'errore.
#ifdef _WIN32 // Su Windows recv richiede il buffer convertito in puntatore a char.
    recv_result = recv(client_socket, (char*)&risposta, sizeof(PacchettoRete), 0); // Attende la risposta iniziale usando Winsock.
#else // Su POSIX il buffer può essere passato direttamente.
    recv_result = recv(client_socket, &risposta, sizeof(PacchettoRete), 0); // Attende la risposta iniziale usando l'API POSIX.
#endif // Termina la selezione della chiamata recv specifica del sistema.
    if (recv_result > 0) { // Prosegue solo se è stato ricevuto almeno un byte.
        printf("\n--- RISPOSTA DAL SERVER ---\n"); // Stampa l'intestazione della risposta.
        printf("Tipo: %d\n", risposta.tipo_messaggio); // Stampa il tipo di messaggio ricevuto.
        printf("Messaggio: %s\n", risposta.payload); // Stampa il testo contenuto nel pacchetto.
        printf("---------------------------\n"); // Stampa la chiusura dell'intestazione.
    }

    // ------------------------------------------
    // --- IL LOOP DI GIOCO INTERATTIVO ---
    // ------------------------------------------
    printf("\nSei nella lobby! Scrivi un messaggio da inviare al server (scrivi 'esci' per terminare):\n"); // Spiega come interagire con il client.
    char input_utente[256]; // Riserva un buffer per il comando o messaggio digitato.
    while (1) { // Ripete la richiesta finché l'utente non esce o si verifica un errore.
        printf("Inserisci comando: "); // Mostra il prompt per l'input.
        if (fgets(input_utente, sizeof(input_utente), stdin) == NULL) { // Legge una riga e rileva fine input o errore.
            break; // Esce dal ciclo se non è stato possibile leggere una riga.
        }
        input_utente[strcspn(input_utente, "\n")] = 0; // Sostituisce il ritorno a capo finale con il terminatore della stringa.
        if (strcmp(input_utente, "esci") == 0) { // Controlla se l'utente ha digitato il comando di uscita.
            break; // Interrompe il ciclo senza inviare il comando al server.
        }
        PacchettoRete nuova_mossa; // Dichiara il pacchetto da inviare per questo input.
        nuova_mossa.tipo_messaggio = 1; // Imposta il tipo di messaggio usato dal client per inviare il testo.
        nuova_mossa.mossa_x = -1; // Indica che il pacchetto non specifica una coordinata x di mossa.
        nuova_mossa.mossa_y = -1; // Indica che il pacchetto non specifica una coordinata y di mossa.
        strcpy(nuova_mossa.payload, input_utente); // Copia nel payload la riga digitata dall'utente.
#ifdef _WIN32 // Su Windows send richiede il buffer convertito in puntatore a char.
        send_result = send(client_socket, (const char*)&nuova_mossa, sizeof(PacchettoRete), 0); // Invia il pacchetto usando Winsock.
#else // Su POSIX si passa direttamente l'indirizzo della struttura.
        send_result = send(client_socket, &nuova_mossa, sizeof(PacchettoRete), 0); // Invia il pacchetto usando l'API POSIX.
#endif // Termina la selezione della chiamata send specifica del sistema.
        if (send_result < 0) { // Verifica se l'invio del messaggio è fallito.
            perror("Errore di invio"); // Stampa il dettaglio dell'errore.
            break; // Interrompe il ciclo perché non è possibile proseguire correttamente.
        }
        PacchettoRete risposta_server; // Dichiara la struttura in cui ricevere la risposta a questo messaggio.
#ifdef _WIN32 // Su Windows recv richiede il buffer convertito in puntatore a char.
        recv_result = recv(client_socket, (char*)&risposta_server, sizeof(PacchettoRete), 0); // Attende la risposta del server con Winsock.
#else // Su POSIX si passa direttamente l'indirizzo della struttura.
        recv_result = recv(client_socket, &risposta_server, sizeof(PacchettoRete), 0); // Attende la risposta del server con l'API POSIX.
#endif // Termina la selezione della chiamata recv specifica del sistema.
        if (recv_result > 0) { // Controlla se il server ha inviato una risposta.
            printf("[Server]: %s\n", risposta_server.payload); // Visualizza il testo ricevuto dal server.
        } else if (recv_result == 0) { // Il valore zero indica che il server ha chiuso la connessione.
            printf("\nIl Server ha chiuso la connessione.\n"); // Informa l'utente che la connessione è stata chiusa.
            break; // Esce dal ciclo perché il server non risponderà più.
        } else {
            perror("Errore nella ricezione"); // Stampa il dettaglio dell'errore di ricezione.
            break; // Interrompe il ciclo in caso di errore.
        }
    }

    // Chiusura pulita quando si esce dal while
#ifdef _WIN32 // Su Windows si chiude il socket tramite Winsock.
    closesocket(client_socket); // Chiude la connessione di rete.
    WSACleanup(); // Rilascia le risorse Winsock inizializzate all'avvio.
#else // Su sistemi POSIX si usa close per chiudere il socket.
    close(client_socket); // Chiude la connessione di rete.
#endif // Termina la pulizia specifica del sistema operativo.
    printf("Ti sei disconnesso.\n"); // Conferma all'utente che la disconnessione è terminata.
    return 0; // Termina main segnalando che il programma è terminato correttamente.
}
