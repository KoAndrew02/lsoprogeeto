# Tris Stellare

Un'applicazione client-server multiplayer del classico gioco del Tris (Tic-Tac-Toe), sviluppata in C. Il progetto supporta stanze di gioco parallele, matchmaking tramite lobby, chat integrata e due tipologie di client: un'interfaccia grafica (GTK3) e un client da terminale.

## Panoramica del Sistema

Il progetto è strutturato secondo il paradigma Client-Server:
*   **Server Centrale (`server/server.c`)**: Un server TCP multithread concorrente. Mantiene lo stato del gioco, autorizza le azioni, elabora le regole (vittoria/pareggio) e gestisce il routing dei messaggi di chat.
*   **Client Grafico (`client_grafico/client_gtk.c`)**: Interfaccia asincrona basata su GTK3. Offre un'esperienza visiva ricca e gestisce l'I/O con l'utente delegando la rete a un thread POSIX dedicato.
*   **Client Terminale (`client_terminale/client.c`)**: Alternativa CLI leggera per interagire con il sistema da riga di comando.
*   **Modulo Condiviso (`shared/shared.h`)**: Header file che contiene la struct `PacchettoRete`, il protocollo applicativo customizzato a dimensione fissa per la comunicazione tra i nodi.

## Tecnologie Utilizzate

*   **Linguaggio C (C99/C11)**: Utilizzato per massimizzare le performance e per il controllo di basso livello.
*   **Socket API (TCP/IP)**: Gestione della comunicazione affidabile in rete.
*   **Pthreads e Mutex**: Gestione della concorrenza sul server per servire client simultanei proteggendo la memoria condivisa.
*   **GTK+ 3 & Cairo**: Framework per la UI e libreria grafica per il rendering procedurale dello sfondo spaziale.
*   **Docker & Docker Compose**: Per il deployment automatizzato e isolato del server.

## Scelte Implementative

*   **Architettura "Server-Authoritative":** I client fungono esclusivamente da terminali di I/O. L'intera logica arbitrale (validazione delle mosse, turnazione, fine partita) è centralizzata sul server per prevenire desincronizzazioni e trucchi (cheat) da parte dei client.
*   **Protocollo a Dimensione Fissa (`PacchettoRete`):** La comunicazione avviene tramite lo scambio di pacchetti binari (struct C) a dimensione fissa, eliminando l'overhead di parsing testuale (es. JSON) e risolvendo i problemi di frammentazione TCP.
*   **Concorrenza Multithread "Detached":** Ogni nuova connessione client viene gestita da un thread indipendente in modalità *detached*, permettendo al sistema operativo di recuperare automaticamente le risorse alla disconnessione e prevenendo memory leak.
*   **Protezione della Memoria Condivisa tramite Mutex:** L'accesso alle strutture dati globali (array `stanze` e `client_connessi`) è rigorosamente sincronizzato tramite `pthread_mutex_lock/unlock` per evitare *race conditions*.
*   **Disaccoppiamento tra Rete e GUI:** Nel client grafico, per evitare il congelamento dell'interfaccia durante le operazioni di rete bloccanti (`recv`), l'ascolto dei socket è demandato a un thread secondario, mentre gli aggiornamenti visivi vengono iniettati nel main loop GTK tramite `g_idle_add`.
*   **Rendering Procedurale degli Asset (Cairo):** Lo sfondo stellato viene generato matematicamente a runtime, permettendo all'interfaccia di essere scalabile e leggera senza dover caricare file immagine raster pesanti.
*   **Compatibilità Cross-Platform:** Il client da terminale utilizza direttive del preprocessore (`#ifdef _WIN32`) per includere dinamicamente `winsock2.h` su Windows o le socket POSIX su Linux/macOS.

---

## Prerequisiti

Per compilare ed eseguire il progetto localmente:
*   **GCC**: Compilatore C standard.
*   **Librerie di sviluppo GTK3**: Necessarie per il client grafico. 
    *   Ubuntu/Debian: `sudo apt install libgtk-3-dev`
    *   Fedora: `sudo dnf install gtk3-devel`
*   **Docker e Docker Compose** (Opzionale, consigliato per il server).

## Esecuzione tramite Docker (Metodo Consigliato)

1. Apri un terminale nella cartella principale del progetto.
2. Avvia i container in background:
   ```bash
   docker-compose up --build -d
   ```
3. Il server sarà in ascolto sulla porta TCP `8080`.
4. Puoi ora lanciare i client grafici o da terminale sulla tua macchina host per connetterti al server (IP `127.0.0.1`).

---

## Compilazione ed Esecuzione Manuale

Apri terminali separati nella directory radice del progetto per compilare e avviare manualmente i moduli.

### 1. Avvio del Server Centrale (Linux/macOS)
```bash
# Compilazione
gcc server/server.c -o server/server -lpthread

# Esecuzione
./server/server
```

### 2. Avvio del Client Grafico GTK
Assicurati di eseguire il comando direttamente dall'interno della cartella del client per permettere al programma di trovare gli asset (immagini png).
```bash
# Entra nella directory
cd client_grafico

# Compilazione
gcc client_gtk.c -o client_gtk `pkg-config --cflags --libs gtk+-3.0` -lpthread -lm

# Esecuzione
./client_gtk
```

### 3. Avvio del Client da Terminale
```bash
# Compilazione (Linux/macOS)
gcc client_terminale/client.c -o client_terminale/client

# Compilazione (Windows - tramite MinGW)
# gcc client_terminale/client.c -o client_terminale/client.exe -lws2_32

# Esecuzione (specificando l'IP del server, default: 127.0.0.1)
./client_terminale/client 127.0.0.1
```

---

## Come Giocare

1. **Lobby Multigiocatore:** All'avvio del client grafico, accederai alla Lobby. La "Bacheca Annunci" mostrerà la creazione o liberazione di stanze da parte del server.
2. **Crea o Partecipa:** 
   * **Crea Nuova Partita**: Apre una stanza e ti imposta come Proprietario (Host).
   * **Unisciti**: Inserisci l'ID numerico di una stanza esistente e clicca il pulsante per lanciare una sfida. L'Host riceverà un pop-up per accettare o rifiutare.
3. **Gameplay:** Il server impone turni rigidi. Il tuo cursore sarà bloccato quando tocca all'avversario. Clicca su una casella vuota per fare la tua mossa.
4. **Chat In-Game:** Usa il box sulla destra per inviare messaggi istantanei al tuo avversario. La chat è privata e limitata alla singola stanza di gioco.
5. **Fine Partita:** Quando un giocatore vince, o la griglia è piena, viene dichiarata la fine della partita. Se uno dei giocatori preme "Esci" o chiude il programma, l'altro rimarrà come nuovo Host e la stanza verrà ripulita e rimessa in bacheca.