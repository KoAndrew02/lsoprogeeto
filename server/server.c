// Fornisce le funzioni di input/output, come printf e sprintf.
#include <stdio.h>
// Fornisce malloc, free e le altre funzioni di gestione della memoria.
#include <stdlib.h>
// Fornisce memset e strncpy per inizializzare e copiare dati.
#include <string.h>
// Fornisce close e le altre funzioni POSIX per i file descriptor.
#include <unistd.h>
// Fornisce le strutture e le funzioni per gli indirizzi Internet.
#include <arpa/inet.h>
// Fornisce thread POSIX e mutex per la concorrenza.
#include <pthread.h>
// Fornisce la gestione dei segnali del processo.
#include <signal.h>
// Condivide con i client porta, limiti e formato dei pacchetti di rete.
#include "../shared/shared.h"

// Rappresenta lo stato e i giocatori di una singola partita.
typedef struct {
    int stato;                 // Stato della stanza: libera, in attesa, in corso o terminata.
    int proprietario;           // Socket del giocatore che ha creato la stanza.
    int giocatore_stella;       // Socket del giocatore che usa il simbolo S.
    int giocatore_luna;         // Socket del giocatore che usa il simbolo L.
    int sfidante_in_attesa;     // Socket dello sfidante in attesa di una risposta dell'host.
    char griglia[3][3];         // Caselle del tris, contenenti S, L o spazio se libere.
    int mosse_fatte;            // Numero di caselle già occupate nella partita corrente.
    int rivincita_stella;       // Vale 1 se il giocatore S ha chiesto la rivincita.
    int rivincita_luna;         // Vale 1 se il giocatore L ha chiesto la rivincita.
} Partita;

Partita stanze[MAX_PARTITE];                         // Elenco condiviso delle stanze di gioco.
int client_connessi[MAX_CLIENTS];                    // Socket dei client attualmente collegati.
pthread_mutex_t mutex_server = PTHREAD_MUTEX_INITIALIZER;  // Protegge lo stato delle stanze.
pthread_mutex_t mutex_clienti = PTHREAD_MUTEX_INITIALIZER; // Protegge l'elenco dei client.

// Imposta lo stato iniziale di tutte le stanze e dell'elenco dei client.
void inizializza_server() {
    // Inizializza una stanza alla volta.
    for(int i=0; i<MAX_PARTITE; i++) {
        // Segna la stanza come libera e senza proprietario o giocatori.
        stanze[i].stato = 0; stanze[i].proprietario = -1;
        stanze[i].giocatore_stella = -1; stanze[i].giocatore_luna = -1;
        // Indica che nessuno sta aspettando una decisione dell'host.
        stanze[i].sfidante_in_attesa = -1;
        // Azzera le richieste di rivincita dei due giocatori.
        stanze[i].rivincita_stella = 0; stanze[i].rivincita_luna = 0;
    }
    // Marca come vuoti tutti gli slot dell'elenco dei client.
    for(int i=0; i<MAX_CLIENTS; i++) client_connessi[i] = 0;
}


/*
La funzione invia_pacchetto() costruisce un pacchetto di rete e lo invia attraverso un socket TCP.
Dichiara la funzione e i parametri ricevuti:

socket: descrittore della connessione verso cui inviare i dati.
tipo: tipo di messaggio da comunicare.
msg: testo da inserire nel pacchetto.
x, y: coordinate della mossa.
id_partita: identificatore della partita.

*/
// Costruisce e invia un pacchetto al socket indicato.
void invia_pacchetto(int socket, int tipo, char* msg, int x, int y, int id_partita) {
    
    if (socket <= 0) return;//Interrompe la funzione se il socket non è valido o non rappresenta una connessione esistente.    
    // Crea il pacchetto e azzera tutti i campi, incluso il testo non utilizzato.
    PacchettoRete p; memset(&p, 0, sizeof(PacchettoRete));
    // Inserisce il tipo di messaggio, le coordinate e l'identificativo della stanza.
    p.tipo_messaggio = tipo; p.mossa_x = x; p.mossa_y = y; p.id_partita = id_partita;
    // Copia il testo senza superare la capacità del campo payload.
    strncpy(p.payload, msg, BUFFER_SIZE - 1);
    // Trasmette il pacchetto completo attraverso il socket TCP.
    send(socket, (char*)&p, sizeof(PacchettoRete), 0);
}

// Invia un annuncio della lobby a tutti i client registrati.
void broadcast_lobby(char *msg) {
    // Impedisce che l'elenco venga modificato mentre lo si attraversa.
    pthread_mutex_lock(&mutex_clienti);
    // Controlla tutti gli slot dell'elenco dei client.
    for(int i=0; i<MAX_CLIENTS; i++) {
        // Invia il messaggio solo agli slot che contengono un socket valido.
        if(client_connessi[i] > 0) invia_pacchetto(client_connessi[i], 20, msg, -1, -1, -1);
    }
    // Rilascia l'elenco per consentire aggiornamenti da altri thread.
    pthread_mutex_unlock(&mutex_clienti);
}

// Restituisce S o L se quel simbolo ha completato una linea; altrimenti restituisce uno spazio.
char controlla_vittoria(int id) {
    // Riferimento alla griglia della stanza, per rendere più brevi i controlli seguenti.
    char (*g)[3] = stanze[id].griglia;
    // Esamina tutte le righe e le colonne.
    for(int i=0; i<3; i++) {
        // Verifica che la riga contenga tre simboli uguali e non sia vuota.
        if((g[i][0] == 'S' || g[i][0] == 'L') && g[i][0] == g[i][1] && g[i][1] == g[i][2]) return g[i][0]; 
        // Verifica che la colonna contenga tre simboli uguali e non sia vuota.
        if((g[0][i] == 'S' || g[0][i] == 'L') && g[0][i] == g[1][i] && g[1][i] == g[2][i]) return g[0][i]; 
    }
    // Verifica la diagonale che va dall'angolo superiore sinistro a quello inferiore destro.
    if((g[0][0] == 'S' || g[0][0] == 'L') && g[0][0] == g[1][1] && g[1][1] == g[2][2]) return g[0][0]; 
    // Verifica la diagonale che va dall'angolo superiore destro a quello inferiore sinistro.
    if((g[0][2] == 'S' || g[0][2] == 'L') && g[0][2] == g[1][1] && g[1][1] == g[2][0]) return g[0][2]; 
    // Nessuna linea vincente è stata trovata.
    return ' '; 
}

// Svuota la griglia e azzera le informazioni della partita appena conclusa.
void resetta_griglia(int id) {
    // Scorre ciascuna delle tre righe.
    for(int r=0; r<3; r++) {
        // Svuota ogni casella della riga corrente.
        for(int c=0; c<3; c++) stanze[id].griglia[r][c] = ' ';
    }
    // Azzera il conteggio delle mosse per la nuova partita.
    stanze[id].mosse_fatte = 0;
    // Rimuove la richiesta di rivincita del giocatore S.
    stanze[id].rivincita_stella = 0;
    // Rimuove la richiesta di rivincita del giocatore L.
    stanze[id].rivincita_luna = 0;
}

// Gestisce l'uscita di un giocatore dalla stanza, riaprendola o chiudendola.
void abbandona_stanza(int socket, int *id_partita) {
    // Legge l'identificativo della stanza associata al client.
    int id = *id_partita;
    // Se il client non è in una stanza valida non c'è nulla da aggiornare.
    if (id < 0 || id >= MAX_PARTITE) return;
    // Protegge lo stato condiviso della stanza durante le modifiche.
    pthread_mutex_lock(&mutex_server);
    
    // Rimuove il socket dal ruolo di giocatore S, se corrisponde.
    if(stanze[id].giocatore_stella == socket) stanze[id].giocatore_stella = -1;
    // Rimuove il socket dal ruolo di giocatore L, se corrisponde.
    if(stanze[id].giocatore_luna == socket) stanze[id].giocatore_luna = -1;
    // Rimuove il socket dal ruolo di proprietario, se corrisponde.
    if(stanze[id].proprietario == socket) stanze[id].proprietario = -1;
    
    // Se entrambi i posti sono vuoti, la stanza non serve più.
    if(stanze[id].giocatore_stella == -1 && stanze[id].giocatore_luna == -1) {
        // Marca la stanza come disponibile.
        stanze[id].stato = 0; 
        // Prepara l'annuncio che comunica la chiusura della stanza.
        char bmsg[BUFFER_SIZE]; sprintf(bmsg, "[TERMINATA] Stanza %d chiusa.", id);
        // Notifica la chiusura a tutti i client della lobby.
        broadcast_lobby(bmsg);
    }
    else {
        // Individua il giocatore rimasto nella stanza.
        int rimasto = (stanze[id].giocatore_stella != -1) ? stanze[id].giocatore_stella : stanze[id].giocatore_luna;
        // Il giocatore rimasto diventa il nuovo proprietario e occupa il posto S.
        stanze[id].proprietario = rimasto; 
        stanze[id].giocatore_stella = rimasto; 
        // Libera il posto L in attesa di un nuovo sfidante.
        stanze[id].giocatore_luna = -1;
        // Riporta la stanza allo stato di attesa di un avversario.
        stanze[id].stato = 2; 
        
        // Pulisce la griglia e le informazioni della partita precedente.
        resetta_griglia(id);
        
        // Comunica al giocatore rimasto che può attendere un nuovo sfidante.
        invia_pacchetto(rimasto, 24, "L'avversario ha abbandonato la partita.\nSei di nuovo in attesa di un nuovo sfidante...", -1, -1, id);
        
        // Prepara l'annuncio che la stanza è nuovamente disponibile.
        char bmsg[BUFFER_SIZE]; sprintf(bmsg, "[LIBERA] Stanza %d di nuovo aperta!", id);
        // Aggiorna la lobby di tutti i client.
        broadcast_lobby(bmsg);
    }
    // Dimentica l'associazione locale alla partita e rilascia il mutex.
    *id_partita = -1; pthread_mutex_unlock(&mutex_server);
}

// Thread dedicato alla gestione di un singolo client collegato.
void *gestisci_client(void *arg) {
    // Recupera il socket passato al thread tramite il puntatore ricevuto.
    int mio_socket = *(int*)arg;
    // Libera l'allocazione effettuata dal thread che ha accettato la connessione.
    free(arg);
    // Indica che questo client non è ancora associato a una partita.
    int mia_partita = -1;

    // Protegge l'elenco mentre si registra il nuovo socket.
    pthread_mutex_lock(&mutex_clienti);
    // Cerca il primo elemento libero nell'elenco dei client connessi.
    for(int i=0; i<MAX_CLIENTS; i++) { if(client_connessi[i] == 0) { client_connessi[i] = mio_socket; break; } }
    // Consente agli altri thread di accedere nuovamente all'elenco.
    pthread_mutex_unlock(&mutex_clienti);

    // Buffer che conterrà il prossimo messaggio ricevuto dal client.
    PacchettoRete p;
    // Continua finché recv riceve dati dal client; la chiusura o un errore termina il ciclo.
    while (recv(mio_socket, (char*)&p, sizeof(PacchettoRete), 0) > 0) {
        // Il simbolo viene determinato in base al ruolo del client nella stanza.
        char mio_simbolo = ' ';
        // Blocca l'accesso mentre si cerca o si legge la partita associata.
        pthread_mutex_lock(&mutex_server);
        // Se l'associazione non è nota localmente, cerca il socket tra i giocatori delle stanze.
        if (mia_partita == -1) {
            // Esamina tutte le stanze disponibili.
            for(int i = 0; i < MAX_PARTITE; i++) {
                // Se il socket appartiene a uno dei due giocatori, memorizza l'indice della stanza.
                if (stanze[i].giocatore_luna == mio_socket || stanze[i].giocatore_stella == mio_socket) {
                    mia_partita = i; break;
                }
            }
        }
        // Se il client è in partita, determina il simbolo assegnatogli.
        if (mia_partita != -1) { 
            mio_simbolo = (stanze[mia_partita].giocatore_stella == mio_socket) ? 'S' : 'L';
        }
        // Rilascia il mutex prima di esaminare il tipo di messaggio.
        pthread_mutex_unlock(&mutex_server);

        // Tipo 10: richiesta di creazione di una nuova stanza.
        if (p.tipo_messaggio == 10) { 
            // Protegge l'array delle stanze mentre viene scelta e inizializzata una stanza.
            pthread_mutex_lock(&mutex_server);
            // Cerca la prima stanza contrassegnata come libera.
            for(int i=0; i<MAX_PARTITE; i++) {
                if(stanze[i].stato == 0) { 
                    // Riserva la stanza e assegna il creatore come proprietario e giocatore S.
                    stanze[i].stato = 2; stanze[i].proprietario = mio_socket; stanze[i].giocatore_stella = mio_socket;
                    // Registra la stanza nella variabile locale del thread e ne azzera la griglia.
                    mia_partita = i; resetta_griglia(i);
                    // Prepara il messaggio di conferma destinato al creatore.
                    char msg[BUFFER_SIZE]; sprintf(msg, "ID Stanza: %d | Sei Host (1° Turno).", i);
                    // Comunica al creatore l'identificativo e il suo ruolo.
                    invia_pacchetto(mio_socket, 14, msg, -1, -1, i); 
                    // Prepara un annuncio per indicare alla lobby la nuova stanza libera.
                    char bmsg[BUFFER_SIZE]; sprintf(bmsg, "[NUOVA CREAZIONE] Stanza %d libera!", i);
                    // Notifica la lobby e interrompe la ricerca, avendo creato la stanza.
                    broadcast_lobby(bmsg); break;
                }
            }
            // Consente agli altri thread di usare o aggiornare le stanze.
            pthread_mutex_unlock(&mutex_server);
        }
        // Tipo 11: richiesta di accesso a una stanza da parte di uno sfidante.
        else if (p.tipo_messaggio == 11) { 
            // Protegge lo stato della stanza richiesta.
            pthread_mutex_lock(&mutex_server);
            // Estrae dal pacchetto l'identificativo della stanza a cui unirsi.
            int id = p.id_partita;
            // Accede alla stanza solo se l'indice rientra nell'array.
            if (id >= 0 && id < MAX_PARTITE) {
                // Una stanza nello stato 2 è libera e attende una sfida.
                if (stanze[id].stato == 2) {
                    // Memorizza lo sfidante finché l'host non accetta o rifiuta.
                    stanze[id].sfidante_in_attesa = mio_socket;
                    // Avvisa il proprietario che è arrivata una richiesta.
                    invia_pacchetto(stanze[id].proprietario, 12, "Qualcuno vuole sfidarti!", -1, -1, id);
                // Una stanza nello stato 3 ha già una partita in corso.
                } else if (stanze[id].stato == 3) {
                    // Comunica allo sfidante che non può entrare in una partita già iniziata.
                    invia_pacchetto(mio_socket, 25, "Errore: Partita in corso!", -1, -1, -1);
                } else {
                    // Informa lo sfidante che la stanza non accetta richieste in questo stato.
                    invia_pacchetto(mio_socket, 25, "Errore: Stanza non disponibile.", -1, -1, -1);
                }
            } else {
                // Segnala che l'identificativo richiesto non corrisponde a una stanza valida.
                invia_pacchetto(mio_socket, 25, "Errore: ID Stanza non valido.", -1, -1, -1);
            }
            // Rilascia il mutex dopo aver concluso la richiesta di accesso.
            pthread_mutex_unlock(&mutex_server);
        }
        // Tipo 13: risposta dell'host alla richiesta di sfida in attesa.
        else if (p.tipo_messaggio == 13) { 
            // Protegge le modifiche a giocatori, stato e sfidante della stanza.
            pthread_mutex_lock(&mutex_server);
            // L'host risponde alla stanza a cui è associato il suo thread.
            int id = mia_partita;
            // Procede solo se la stanza esiste e ha uno sfidante in attesa.
            if (id != -1 && stanze[id].sfidante_in_attesa != -1) {
                // Salva il socket dello sfidante prima di aggiornare la stanza.
                int sfidante = stanze[id].sfidante_in_attesa;
                // Il valore 1 nella coordinata x indica l'accettazione della sfida.
                if (p.mossa_x == 1) { 
                    // Assegna lo sfidante al ruolo L e segna la partita come in corso.
                    stanze[id].giocatore_luna = sfidante; stanze[id].stato = 3; 
                    // Informa lo sfidante che è entrato e giocherà per secondo.
                    invia_pacchetto(sfidante, 14, "Entrato! (2° Turno).", -1, -1, id); 
                    // Informa l'host che la partita è iniziata e che tocca a lui.
                    invia_pacchetto(mio_socket, 2, "Iniziata! Tocca a te.", -1, -1, id);
                    
                    // Prepara l'annuncio che segnala la partita attiva nella lobby.
                    char bmsg[BUFFER_SIZE]; sprintf(bmsg, "[IN CORSO] Stanza %d occupata!", id);
                    // Invia l'annuncio a tutti i client della lobby.
                    broadcast_lobby(bmsg);
                } else {
                    // Se l'host non ha accettato, comunica il rifiuto allo sfidante.
                    invia_pacchetto(sfidante, 25, "Il creatore ha rifiutato la sfida.", -1, -1, -1);
                }
                // Azzera la richiesta in entrambi i casi, accettazione o rifiuto.
                stanze[id].sfidante_in_attesa = -1;
            }
            // Rilascia il mutex al termine della risposta dell'host.
            pthread_mutex_unlock(&mutex_server);
        }
        // Tipo 1: richiesta di una mossa da parte di un giocatore associato a una partita.
        else if (p.tipo_messaggio == 1 && mia_partita != -1) { 
            // Protegge la griglia e gli aggiornamenti di stato della partita.
            pthread_mutex_lock(&mutex_server);
            // Recupera l'indice della stanza interessata.
            int id = mia_partita;
            // Individua il socket dell'avversario in base al simbolo del giocatore corrente.
            int avv = (mio_simbolo == 'S') ? stanze[id].giocatore_luna : stanze[id].giocatore_stella;

            // Verifica che la casella scelta sia ancora vuota.
            if (stanze[id].griglia[p.mossa_x][p.mossa_y] != ' ') {
                // Rifiuta la mossa e restituisce al mittente le coordinate della casella.
                invia_pacchetto(mio_socket, 2, "Casella gia occupata!", p.mossa_x, p.mossa_y, id);
                // Rilascia il mutex prima di terminare la gestione di questa mossa.
                pthread_mutex_unlock(&mutex_server);
            } else {
                // Scrive il simbolo del giocatore nella casella selezionata.
                stanze[id].griglia[p.mossa_x][p.mossa_y] = mio_simbolo;
                // Aggiorna il numero complessivo delle mosse effettuate.
                stanze[id].mosse_fatte++;
                // Controlla se la mossa ha completato una linea vincente.
                char v = controlla_vittoria(id);
                // La partita termina se qualcuno ha vinto o se sono state occupate tutte le caselle.
                if (v != ' ' || stanze[id].mosse_fatte == 9) {
                    // Marca la stanza come terminata.
                    stanze[id].stato = 4;
                    // Se è stata trovata una linea, invia i risultati distinti ai giocatori.
                    if(v != ' ') {
                        // Notifica la vittoria al giocatore che ha appena mosso.
                        invia_pacchetto(mio_socket, 3, "Vinto!", p.mossa_x, p.mossa_y, id);
                        // Notifica la sconfitta all'avversario.
                        invia_pacchetto(avv, 4, "Perso!", p.mossa_x, p.mossa_y, id);
                    }
                    else {
                        // Se non ci sono caselle libere e nessuno ha vinto, comunica il pareggio a entrambi.
                        invia_pacchetto(mio_socket, 5, "Pareggio!", p.mossa_x, p.mossa_y, id);
                        invia_pacchetto(avv, 5, "Pareggio!", p.mossa_x, p.mossa_y, id);
                    }
                } else {
                    // Se la partita continua, passa il turno all'avversario.
                    invia_pacchetto(avv, 1, "Tocca a te!", p.mossa_x, p.mossa_y, id);
                }
                // Rilascia il mutex al termine dell'elaborazione della mossa.
                pthread_mutex_unlock(&mutex_server);
            }
        }
        // Tipo 21: messaggio di chat da inoltrare all'avversario nella stessa partita.
        else if (p.tipo_messaggio == 21 && mia_partita != -1) { 
            // Protegge la lettura dei dati della stanza durante l'inoltro.
            pthread_mutex_lock(&mutex_server);
            // Recupera l'indice della partita del mittente.
            int id = mia_partita;
            // Seleziona il socket dell'altro partecipante.
            int avv = (mio_simbolo == 'S') ? stanze[id].giocatore_luna : stanze[id].giocatore_stella;
            // Inoltra il testo solo se l'avversario è ancora collegato alla stanza.
            if (avv != -1) invia_pacchetto(avv, 21, p.payload, -1, -1, id);
            // Rilascia il mutex dopo l'inoltro.
            pthread_mutex_unlock(&mutex_server);
        }
        // Tipo 22: richiesta di rivincita da parte di uno dei giocatori.
        else if (p.tipo_messaggio == 22 && mia_partita != -1) {
            // Protegge i flag di rivincita e i dati della partita.
            pthread_mutex_lock(&mutex_server);
            // Recupera l'indice della partita in cui è arrivata la richiesta.
            int id = mia_partita;
            
            // Registra la richiesta di rivincita del giocatore S.
            if (mio_simbolo == 'S') stanze[id].rivincita_stella = 1;
            // Registra la richiesta di rivincita del giocatore L.
            else if (mio_simbolo == 'L') stanze[id].rivincita_luna = 1;

            // Riavvia la partita solo quando entrambi hanno accettato la rivincita.
            if (stanze[id].rivincita_stella == 1 && stanze[id].rivincita_luna == 1) {
                // Azzera la griglia e i flag della partita precedente.
                resetta_griglia(id);
                // Segna la stanza come nuovamente in corso.
                stanze[id].stato = 3;
                // Comunica al giocatore S che inizia la rivincita.
                invia_pacchetto(stanze[id].giocatore_stella, 23, "Rivincita! Tocca a te.", -1, -1, id);
                // Comunica al giocatore L che deve attendere il primo turno.
                invia_pacchetto(stanze[id].giocatore_luna, 23, "Rivincita! Attendi la mossa.", -1, -1, id);
            }
            // Rilascia il mutex al termine della gestione della richiesta.
            pthread_mutex_unlock(&mutex_server);
        }
        // Tipo 26: inoltra all'avversario la richiesta o l'aggiornamento della skin.
        else if (p.tipo_messaggio == 26 && mia_partita != -1) {
            // Protegge i dati condivisi mentre individua l'avversario.
            pthread_mutex_lock(&mutex_server);
            // Recupera l'indice della partita.
            int id = mia_partita;
            // Individua l'altro giocatore in base al simbolo del mittente.
            int avv = (mio_simbolo == 'S') ? stanze[id].giocatore_luna : stanze[id].giocatore_stella;
            // Inoltra il payload se l'avversario è presente.
            if (avv != -1) invia_pacchetto(avv, 26, p.payload, -1, -1, id);
            // Rilascia il mutex dopo l'inoltro.
            pthread_mutex_unlock(&mutex_server);
        }
        // Tipo 16: il client dichiara di voler abbandonare la stanza.
        else if (p.tipo_messaggio == 16) abbandona_stanza(mio_socket, &mia_partita);
        // Azzera il pacchetto prima della successiva ricezione.
        memset(&p, 0, sizeof(PacchettoRete));
    }
    // Se il ciclo è terminato per disconnessione o errore, aggiorna la stanza.
    abbandona_stanza(mio_socket, &mia_partita);
    // Protegge l'elenco dei client mentre si rimuove il socket disconnesso.
    pthread_mutex_lock(&mutex_clienti);
    // Cerca il socket nella lista e libera lo slot corrispondente.
    for(int i=0; i<MAX_CLIENTS; i++) { if(client_connessi[i] == mio_socket) { client_connessi[i] = 0; break; } }
    // Rilascia l'elenco dei client.
    pthread_mutex_unlock(&mutex_clienti);
    // Chiude il socket e termina il thread restituendo NULL.
    close(mio_socket); return NULL;
}

// Punto di ingresso: configura il socket di ascolto e avvia un thread per ogni client.
int main() {
    // Disabilita il buffering dell'output e ignora SIGPIPE per evitare la terminazione su socket chiusi.
    setbuf(stdout, NULL); signal(SIGPIPE, SIG_IGN); inizializza_server();
    // Crea un socket TCP IPv4 e prepara l'opzione di riutilizzo della porta.
    int s = socket(AF_INET, SOCK_STREAM, 0); int opt = 1;
    // Consente di riutilizzare subito la porta locale dopo il riavvio del server.
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // Inizializza la struttura dell'indirizzo IPv4 del server.
    struct sockaddr_in addr; addr.sin_family = AF_INET;
    // Imposta la porta configurata e accetta connessioni su tutte le interfacce locali.
    addr.sin_port = htons(PORT); addr.sin_addr.s_addr = INADDR_ANY;
    // Associa il socket alla porta e lo mette in ascolto con una coda massima di 10 connessioni.
    bind(s, (struct sockaddr*)&addr, sizeof(addr)); listen(s, 10);
    // Stampa il messaggio di avvio del server.
    printf("Server Tris v16 (Custom Skin Attive) in ascolto...\n");
    // Rimane in ascolto per accettare nuove connessioni.
    while (1) {
        // Prepara la struttura che riceverà l'indirizzo del client e la sua dimensione.
        struct sockaddr_in c_addr; socklen_t size = sizeof(c_addr);
        // Accetta una connessione e ottiene il socket dedicato al nuovo client.
        int client_s = accept(s, (struct sockaddr*)&c_addr, &size);
        // Alloca memoria per passare il socket al thread di gestione.
        int *new_sock = malloc(sizeof(int)); *new_sock = client_s;
        // Crea un thread per il client e lo rende detached per liberare automaticamente le risorse.
        pthread_t t; pthread_create(&t, NULL, gestisci_client, (void*)new_sock); pthread_detach(t);
    }
    // Punto di uscita previsto dal tipo di ritorno di main; il ciclo normalmente non termina.
    return 0;
}