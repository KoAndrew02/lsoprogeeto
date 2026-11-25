# 🌙 Tris Stellare ⭐

Tris (Tic-Tac-Toe) multiplayer client-server scritto in C: stanze di gioco parallele, lobby con bacheca annunci in tempo reale, chat di partita, scelta dell'avatar, rivincita e fuochi d'artificio per chi vince.

## Struttura del progetto

```
.
├── client_grafico/
│   ├── client_gtk.c       # Client grafico GTK3 (Windows con Winsock2, macOS/Linux con socket POSIX)
│   ├── client_gtk.exe     # Build Windows di riferimento
│   ├── stella.png         # Skin: Stella Classica
│   ├── stella2.png        # Skin: Stella Speciale
│   ├── luna.png           # Skin: Luna Classica
│   └── luna2.png          # Skin: Luna Scura
├── server/
│   ├── server.c           # Server TCP multithread
│   └── Dockerfile         # Immagine Docker del server
├── shared/
│   └── shared.h           # Protocollo comune (porta e struct PacchettoRete)
├── docs/
│   └── spiegazione.txt    # Descrizione dettagliata dei moduli
└── docker-compose.yml     # Avvio del server con Docker
```

## Componenti

* **Server (`server/server.c`)**: server TCP concorrente. Crea un thread *detached* per ogni client e protegge le strutture condivise (`stanze`, `client_connessi`) con due mutex. Mantiene lo stato autorevole della partita: valida le mosse, gestisce i turni, rileva vittoria e pareggio, inoltra chat e skin, coordina rivincite e abbandoni.
* **Client grafico (`client_grafico/client_gtk.c`)**: interfaccia GTK3. Un thread secondario riceve i pacchetti dal server e li passa al main loop GTK con `g_idle_add`, così la finestra non si blocca mai.
  * Sfondo animato con Cairo: stelle che scendono e una luna che attraversa il cielo cambiando fase.
  * Fuochi d'artificio a particelle quando vinci, tris vincente evidenziato in verde.
  * Scelta dell'avatar a inizio partita fra 4 skin, sincronizzata con l'avversario.
  * Dialoghi gialli "a nuvola" per sfide, errori, fine partita e rivincita.
* **Protocollo (`shared/shared.h`)**: ogni messaggio è una `PacchettoRete` a dimensione fissa (tipo, coordinate, id partita, payload di 256 byte). Il client ricompone i pacchetti anche se TCP li spezza.

### Tipi di messaggio

| Tipo | Significato | Tipo | Significato |
|---|---|---|---|
| 1 | Mossa | 16 | Uscita dalla stanza |
| 2 | Partita iniziata (all'host) | 20 | Annuncio in bacheca |
| 3 / 4 / 5 | Vittoria / sconfitta / pareggio | 21 | Chat |
| 10 | Crea stanza | 22 / 23 | Richiesta / avvio rivincita |
| 11 | Unisciti a una stanza | 24 | Avversario uscito, si torna in attesa |
| 12 / 13 | Sfida in arrivo / risposta | 25 | Errore |
| 14 | Ingresso in partita | 26 | Skin scelta |

## Avvio

### 1. Server (Docker, consigliato)

Dalla cartella principale del progetto:
```bash
docker compose up --build -d
```
Il server resta in ascolto sulla porta TCP `8081`. Per fermarlo: `docker compose down`.

In alternativa, su Linux o macOS senza Docker:
```bash
gcc server/server.c -o server/server -lpthread
./server/server
```

### 2. Client grafico (Windows)

Serve [MSYS2](https://www.msys2.org/). Nel terminale **MSYS2 MinGW64** installa una volta compilatore e GTK3:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk3 mingw-w64-x86_64-pkgconf
```
Poi compila e avvia **dalla cartella `client_grafico`** (il client cerca le immagini delle skin nella cartella da cui parte):
```bash
cd client_grafico
gcc client_gtk.c -o client_gtk.exe $(pkg-config --cflags --libs gtk+-3.0) -lws2_32 -lpthread -lm
./client_gtk.exe
```
### 3. Client grafico (macOS)

Installa una volta GTK3 con [Homebrew](https://brew.sh/):
```bash
brew install gtk+3 pkgconf
```
Poi, sempre dalla cartella `client_grafico`:
```bash
cd client_grafico
gcc client_gtk.c -o client_gtk $(pkg-config --cflags --libs gtk+-3.0) -lpthread -lm
./client_gtk
```
Lo stesso comando funziona anche su Linux (con `libgtk-3-dev` installato).

Il client si collega al server su `127.0.0.1:8081`, quindi server e client devono girare sullo stesso computer. Per giocare apri due client.

## Come si gioca

1. **Lobby**: la bacheca mostra in tempo reale le stanze create, occupate e liberate.
2. **Crea Nuova Partita**: apri una stanza e diventi l'host (Stella, primo turno).
3. **Unisciti**: inserisci l'ID di una stanza e sfida l'host, che può accettare o rifiutare. Chi entra gioca come Luna.
4. **Avatar**: a inizio partita entrambi scelgono la propria skin fra Stella Classica, Stella Speciale, Luna Classica e Luna Scura.
5. **Partita**: il server impone i turni, la griglia è bloccata quando tocca all'avversario. A destra c'è la chat privata della stanza.
6. **Fine partita**: il tris vincente si colora di verde e chi vince vede i fuochi d'artificio. Si può chiedere la rivincita: riparte solo se accettano entrambi, a turni invertiti.
7. **Abbandono**: se l'avversario esce, chi resta torna host e la stanza ritorna in bacheca.
