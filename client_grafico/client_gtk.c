/*
    Client grafico GTK di Tris Stellare.
    Il sorgente corrisponde al client_gtk.exe presente in questa cartella (ricostruito dall'eseguibile):
    stesse funzioni, stesse stringhe, stesse costanti e stessa logica.
    Contiene: sfondo animato (stelle che scendono e luna che attraversa il cielo cambiando fase),
    fuochi d'artificio alla vittoria, evidenziazione del tris vincente, scelta dell'avatar,
    rivincita e finestre di dialogo gialle a forma di "nuvola".
*/
#include <gtk/gtk.h>       // Importa le funzioni GTK usate per creare la finestra e i widget.
#include <pthread.h>       // Permette di creare il thread che ascolta i messaggi del server.
#include <stdio.h>         // Fornisce funzioni di input/output formattato, come sprintf.
#include <stdlib.h>        // Fornisce funzioni generali, come malloc, free, atoi e rand.
#include <string.h>        // Fornisce funzioni per manipolare stringhe e memoria.
#include <unistd.h>        // Importa funzioni POSIX; viene incluso anche se qui non è usato direttamente.
#ifdef _WIN32              // Su Windows (MSYS2/MinGW)...
#include <winsock2.h>      // ...fornisce le API Windows per la comunicazione tramite socket.
#else                      // Su macOS e Linux...
#include <sys/socket.h>    // ...usa le socket POSIX (socket, connect, send, recv).
#include <netinet/in.h>    // Definisce struct sockaddr_in e htons.
#include <arpa/inet.h>     // Definisce inet_addr.
#endif
#include <math.h>          // Fornisce costanti e funzioni matematiche, tra cui M_PI, cos e sin.
#include "../shared/shared.h" // Condivide porta e formato dei pacchetti di rete con client e server.

// Raggruppa le coordinate di una casella e il pulsante GTK che la rappresenta.
typedef struct {
    int r;                 // Memorizza la riga della casella nella griglia.
    int c;                 // Memorizza la colonna della casella nella griglia.
    GtkWidget *b;          // Punta al pulsante GTK associato alla casella.
} DatiB;

// --- STELLE DELLO SFONDO ---
#define NUM_STELLE 120     // Numero di stelle disegnate nello sfondo.
typedef struct {
    double x;              // Posizione orizzontale della stella.
    double y;              // Posizione verticale della stella.
    double speed;          // Velocità con cui la stella scende verso il basso.
    int color;             // 0 = stella bianca e piccola, 1 = stella gialla e più grande.
} StellaSfondo;
StellaSfondo stelle[NUM_STELLE]; // Array con tutte le stelle dello sfondo.
int stelle_init = 0;       // Vale 1 dopo che le stelle sono state posizionate la prima volta.

double luna_x = -1000;     // Posizione orizzontale della luna; il valore iniziale viene sostituito al primo disegno.
int luna_fase = 0;         // Fase lunare corrente (da 0 a 7), cambia ogni volta che la luna esce dallo schermo.

// --- FUOCHI D'ARTIFICIO ---
#define NUM_FUOCHI 200     // Numero massimo di particelle dei fuochi presenti contemporaneamente.
typedef struct {
    double x, y;           // Posizione della particella.
    double vx, vy;         // Velocità orizzontale e verticale della particella.
    double life;           // Vita residua: 1 = appena nata e opaca, 0 = sparita.
    double r, g, b;        // Colore della particella.
    int active;            // Vale 1 se la particella è in uso, 0 se è libera.
} ParticellaFuoco;
ParticellaFuoco fuochi[NUM_FUOCHI]; // Insieme di particelle riutilizzate da tutte le esplosioni.
int vittoria_fuochi = 0;   // Vale 1 quando il giocatore ha vinto e i fuochi devono essere mostrati.

int client_socket;         // Socket usato per inviare e ricevere dati dal server.
int id_p = -1;             // ID della partita corrente; -1 indica che non è ancora noto.

char mia_skin[50] = "stella.png";      // Immagine usata per le proprie mosse.
char avversario_skin[50] = "luna.png"; // Immagine usata per le mosse dell'avversario, comunicata dal server.
int mio_t = 0;             // Vale 1 quando è il turno del giocatore e 0 quando non lo è.
char board_locale[3][3] = { {' ',' ',' '}, {' ',' ',' '}, {' ',' ',' '} }; // Copia locale della griglia: 'M' = mia mossa, 'A' = avversario, ' ' = vuota.

GtkWidget *win, *stack, *lbl_s, *grid_g, *entry_id, *entry_chat; // Puntatori ai widget GTK condivisi tra le callback.
GtkTextBuffer *buf_lobby, *buf_chat; // Buffer di testo usati per aggiornare lobby e chat.
DatiB griglia[3][3];       // Dati delle nove caselle del tris.

GtkWidget *dialogo_corrente = NULL; // Dialogo di fine partita aperto in questo momento (serve per chiuderlo se l'avversario esce).

// --- SCELTA AVATAR ---
    /*
        Mostra un dialogo con un menu a tendina da cui scegliere l'avatar tra 4 skin.
        La skin scelta viene salvata in mia_skin e inviata al server (tipo 26),
        che la inoltra all'avversario.
    */
void scegli_skin() {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_OTHER, GTK_BUTTONS_OK, NULL); // Crea il dialogo modale con il solo pulsante OK.
    gtk_window_set_decorated(GTK_WINDOW(d), FALSE); // Toglie la barra del titolo per l'aspetto a "nuvola".
    gtk_widget_set_name(d, "nuvola-alert"); // Assegna il nome usato dalle regole CSS del dialogo giallo.
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(d), "<span foreground='black' weight='bold' size='large'>Scegli il tuo Avatar!</span>"); // Imposta il titolo del dialogo in grassetto.

    GtkWidget *area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(d)); // Recupera la zona del dialogo dove inserire altri widget.
    GtkWidget *combo = gtk_combo_box_text_new(); // Crea il menu a tendina.
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "⭐ Stella Classica"); // Voce 0: stella.png.
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "🌟 Stella Speciale"); // Voce 1: stella2.png.
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "🌙 Luna Classica");   // Voce 2: luna.png.
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "🌑 Luna Scura");      // Voce 3: luna2.png.
    if (mio_t == 1) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0); // Chi inizia (Stella) trova preselezionata la stella classica.
    else gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 2); // Chi gioca per secondo (Luna) trova preselezionata la luna classica.
    gtk_container_add(GTK_CONTAINER(area), combo); // Inserisce il menu a tendina nel dialogo.
    gtk_widget_show_all(d); // Rende visibili il dialogo e il menu.
    gtk_dialog_run(GTK_DIALOG(d)); // Mostra il dialogo e attende la pressione di OK.

    gchar *scelta = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo)); // Legge il testo della voce selezionata.
    if (scelta) { // Procede solo se è stata selezionata una voce.
        if (strstr(scelta, "Stella Classica")) strcpy(mia_skin, "stella.png");       // Associa la voce al file della stella classica.
        else if (strstr(scelta, "Stella Speciale")) strcpy(mia_skin, "stella2.png"); // Associa la voce al file della stella speciale.
        else if (strstr(scelta, "Luna Classica")) strcpy(mia_skin, "luna.png");      // Associa la voce al file della luna classica.
        else if (strstr(scelta, "Luna Scura")) strcpy(mia_skin, "luna2.png");        // Associa la voce al file della luna scura.
        g_free(scelta); // Libera la stringa restituita da GTK.
    }
    gtk_widget_destroy(d); // Chiude e libera il dialogo.

    PacchettoRete p_skin; // Dichiara il pacchetto con cui comunicare la skin al server.
    memset(&p_skin, 0, sizeof(p_skin)); // Azzera tutti i campi del pacchetto.
    p_skin.tipo_messaggio = 26; // Indica al server che il pacchetto contiene la skin scelta.
    strcpy(p_skin.payload, mia_skin); // Copia il nome del file della skin nel payload.
    send(client_socket, (char*)&p_skin, sizeof(p_skin), 0); // Invia la skin al server.
}

// --- DISEGNO SFONDO (STESSO PER TUTTO) ---
    /*
        GtkWidget *w: il widget che viene ridisegnato, ovvero l’GtkDrawingArea da.
        cairo_t *cr: il contesto Cairo usato per disegnare colori, cerchi e altre forme.
        gpointer d: dati aggiuntivi eventualmente associati al segnale
        Viene richiamata ogni 33 ms da timer_sfondo, quindi ogni chiamata è un fotogramma dell'animazione.
    */
gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
    int width = gtk_widget_get_allocated_width(w); // Legge la larghezza disponibile per disegnare lo sfondo.
    int height = gtk_widget_get_allocated_height(w); // Legge l'altezza disponibile per disegnare lo sfondo.

    if (!stelle_init && width > 1) { // Al primo disegno con una finestra valida posiziona le stelle.
        for (int i = 0; i < NUM_STELLE; i++) {
            stelle[i].x = rand() % width; // Sceglie la coordinata orizzontale casuale della stella.
            stelle[i].y = rand() % height; // Sceglie la coordinata verticale casuale della stella.
            stelle[i].speed = (rand() % 3) * 0.5 + 0.5; // Sceglie una velocità tra 0.5, 1.0 e 1.5 pixel per fotogramma.
            stelle[i].color = rand() % 2; // Sceglie a caso se la stella è bianca o gialla.
        }
        luna_x = width + 100; // Fa partire la luna appena fuori dal bordo destro.
        stelle_init = 1; // Segna che l'inizializzazione è stata fatta.
    }

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.1); // Imposta un colore blu molto scuro.
    cairo_paint(cr); // Riempie tutta l'area con il colore impostato.

    if (stelle_init) { // Disegna e muove le stelle solo se sono state inizializzate.
        for (int i = 0; i < NUM_STELLE; i++) {
            if (stelle[i].color == 0) cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // Colore bianco.
            else cairo_set_source_rgb(cr, 1.0, 1.0, 0.5); // Colore giallo chiaro.
            cairo_arc(cr, stelle[i].x, stelle[i].y, (stelle[i].color)+1, 0, 2*M_PI); // Definisce un cerchio di raggio 1 o 2 nella posizione della stella.
            cairo_fill(cr); // Riempie il cerchio per disegnare la stella.
            stelle[i].y += stelle[i].speed; // Sposta la stella verso il basso.
            if (stelle[i].y > height) { // Se la stella esce dal bordo inferiore...
                stelle[i].y = 0; // ...la riporta in cima...
                stelle[i].x = rand() % width; // ...in una nuova posizione orizzontale casuale.
            }
        }
    }

    luna_x -= 4.0; // Sposta la luna di 4 pixel verso sinistra a ogni fotogramma.
    if (luna_x < -150) { // Se la luna è uscita dal bordo sinistro...
        luna_x = width + 150; // ...la riporta oltre il bordo destro...
        luna_fase = (luna_fase + 1) % 8; // ...e passa alla fase lunare successiva.
    }
    int offset_fasi[] = { 200, -50, -25, -10, 0, 10, 25, 50 }; // Spostamento del cerchio scuro per ogni fase (200 = luna piena, 0 = luna nuova).
    cairo_set_source_rgb(cr, 1.0, 0.9, 0.3); // Imposta il colore giallo della luna.
    cairo_arc(cr, luna_x, 120, 70, 0, 2*M_PI); // Disegna il cerchio grande che forma la luna.
    cairo_fill(cr); // Riempie il cerchio giallo.
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.1); // Ripristina il colore dello sfondo per creare la fase.
    cairo_arc(cr, luna_x + offset_fasi[luna_fase], 120, 72, 0, 2*M_PI); // Disegna un cerchio scuro spostato in base alla fase.
    cairo_fill(cr); // Copre parte del cerchio giallo e lascia visibile la fase lunare.

    if (vittoria_fuochi) { // Fuochi d'artificio: attivi solo dopo una vittoria (messaggio tipo 3).
        if (rand() % 15 == 0) { // In media una nuova esplosione ogni 15 fotogrammi (circa 2 al secondo).
            double cx = 100 + rand() % (width - 200); // Centro orizzontale dell'esplosione, lontano dai bordi.
            double cy = 50 + rand() % (height / 2); // Centro verticale dell'esplosione, nella metà alta della finestra.
            double col_r = (rand() % 100) / 100.0; // Componente rossa casuale del colore.
            double col_g = (rand() % 100) / 100.0; // Componente verde casuale del colore.
            double col_b = (rand() % 100) / 100.0; // Componente blu casuale del colore.
            if (col_r < 0.3 && col_g < 0.3 && col_b < 0.3) { col_r = 1.0; col_g = 1.0; } // Se il colore è troppo scuro lo schiarisce, altrimenti non si vedrebbe sul cielo.

            int spawnate = 0; // Conta le particelle create per questa esplosione.
            for (int i = 0; i < NUM_FUOCHI && spawnate < 40; i++) { // Usa al massimo 40 particelle libere.
                if (!fuochi[i].active) { // Considera solo le particelle libere.
                    fuochi[i].x = cx; // Tutte le particelle partono dal centro dell'esplosione.
                    fuochi[i].y = cy;
                    double angolo = (rand() % 360) * M_PI / 180.0; // Direzione casuale, convertita da gradi a radianti.
                    double speed = (rand() % 100) / 20.0 + 1.0; // Velocità casuale tra 1 e 6 pixel per fotogramma.
                    fuochi[i].vx = cos(angolo) * speed; // Componente orizzontale della velocità.
                    fuochi[i].vy = sin(angolo) * speed; // Componente verticale della velocità.
                    fuochi[i].life = 1.0; // La particella nasce completamente opaca.
                    fuochi[i].r = col_r; // Tutte le particelle dell'esplosione hanno lo stesso colore.
                    fuochi[i].g = col_g;
                    fuochi[i].b = col_b;
                    fuochi[i].active = 1; // Segna la particella come in uso.
                    spawnate++; // Aggiorna il conteggio delle particelle create.
                }
            }
        }
        for (int i = 0; i < NUM_FUOCHI; i++) { // Disegna e aggiorna tutte le particelle attive.
            if (fuochi[i].active) {
                cairo_set_source_rgba(cr, fuochi[i].r, fuochi[i].g, fuochi[i].b, fuochi[i].life); // Colore della particella con trasparenza pari alla vita residua.
                cairo_arc(cr, fuochi[i].x, fuochi[i].y, 2.5, 0, 2*M_PI); // Definisce un pallino di raggio 2.5.
                cairo_fill(cr); // Disegna il pallino.
                fuochi[i].x += fuochi[i].vx; // Sposta la particella in orizzontale.
                fuochi[i].y += fuochi[i].vy; // Sposta la particella in verticale.
                fuochi[i].vy += 0.15; // Gravità: la traiettoria curva verso il basso.
                fuochi[i].life -= 0.02; // La particella svanisce (dura circa 50 fotogrammi).
                if (fuochi[i].life <= 0) fuochi[i].active = 0; // Quando è sparita la particella torna libera.
            }
        }
    }
    return FALSE; // Comunica a GTK che questa callback non richiede altri disegnatori predefiniti.
}

// Viene chiamata ogni 33 ms (circa 30 fotogrammi al secondo) e chiede di ridisegnare lo sfondo.
gboolean timer_sfondo(gpointer widget) {
    gtk_widget_queue_draw(GTK_WIDGET(widget)); // Richiede a GTK di rieseguire on_draw.
    return TRUE; // Restituendo TRUE il timer continua a ripetersi.
}

    /*
        La funzione serve a modificare il contenuto visibile di un pulsante della griglia,
         sostituendo il suo testo o contenuto vuoto con l’immagine della skin.
    */
void imposta_img(GtkWidget *b, const char *path) {
    GdkPixbuf *p = gdk_pixbuf_new_from_file(path, NULL); // Carica da disco l'immagine indicata dal percorso.
    if (!p) return; // Interrompe la funzione se il caricamento dell'immagine non è riuscito.
    GdkPixbuf *s = gdk_pixbuf_scale_simple(p, 60, 60, GDK_INTERP_BILINEAR); // Ridimensiona l'immagine a 60x60 pixel.
    gtk_button_set_image(GTK_BUTTON(b), gtk_image_new_from_pixbuf(s)); // Mostra l'immagine ridimensionata nel pulsante.
    g_object_unref(p); // Rilascia il riferimento al pixbuf originale.
    g_object_unref(s); // Rilascia il riferimento al pixbuf ridimensionato.
}

// Aggiunge la classe CSS "vincente" (sfondo verde) alle tre caselle del tris.
void colora_vincenti(int r1, int c1, int r2, int c2, int r3, int c3) {
    GtkStyleContext *ctx; // Contesto di stile del pulsante da colorare.
    ctx = gtk_widget_get_style_context(griglia[r1][c1].b); // Recupera lo stile della prima casella.
    gtk_style_context_add_class(ctx, "vincente"); // La colora di verde.
    ctx = gtk_widget_get_style_context(griglia[r2][c2].b); // Recupera lo stile della seconda casella.
    gtk_style_context_add_class(ctx, "vincente"); // La colora di verde.
    ctx = gtk_widget_get_style_context(griglia[r3][c3].b); // Recupera lo stile della terza casella.
    gtk_style_context_add_class(ctx, "vincente"); // La colora di verde.
}

// Cerca nella griglia locale la riga, colonna o diagonale vincente e la evidenzia.
void trova_e_colora_tris() {
    for(int i=0; i<3; i++) {
        if(board_locale[i][0] != ' ' && board_locale[i][0] == board_locale[i][1] && board_locale[i][1] == board_locale[i][2]) { colora_vincenti(i,0, i,1, i,2); return; } // Controlla la riga i.
        if(board_locale[0][i] != ' ' && board_locale[0][i] == board_locale[1][i] && board_locale[1][i] == board_locale[2][i]) { colora_vincenti(0,i, 1,i, 2,i); return; } // Controlla la colonna i.
    }
    if(board_locale[0][0] != ' ' && board_locale[0][0] == board_locale[1][1] && board_locale[1][1] == board_locale[2][2]) { colora_vincenti(0,0, 1,1, 2,2); return; } // Controlla la diagonale principale.
    if(board_locale[0][2] != ' ' && board_locale[0][2] == board_locale[1][1] && board_locale[1][1] == board_locale[2][0]) { colora_vincenti(0,2, 1,1, 2,0); return; } // Controlla la diagonale secondaria.
}

// --- AZIONI CHAT ---
    /*
          È una callback GTK collegata normalmente al campo di testo della chat.
          e rappresenta il widget GtkEntry, cioè la casella in cui l’utente ha scritto.
          d è un parametro opzionale trasmesso dal sistema GTK; qui non viene utilizzato.
    */
void invia_chat(GtkEntry *e, gpointer d) {
    const char *txt = gtk_entry_get_text(e); // Legge il testo scritto nella casella della chat.
    if(strlen(txt) == 0) return; // Non invia nulla se il campo è vuoto.
    PacchettoRete p; // Dichiara il pacchetto da inviare al server.
    memset(&p, 0, sizeof(p)); // Inizializza a zero tutti i campi del pacchetto.
    p.tipo_messaggio = 21; // Indica al server che il pacchetto contiene un messaggio di chat.
    strcpy(p.payload, txt); // Copia il testo digitato nel campo dati del pacchetto.
    send(client_socket, (char*)&p, sizeof(p), 0); // Invia il pacchetto attraverso il socket.
    GtkTextIter iter; // Dichiara un iteratore per individuare una posizione nel buffer della chat.
    gtk_text_buffer_get_end_iter(buf_chat, &iter); // Posiziona l'iteratore alla fine della chat visibile.
    char me[300]; // Prepara un buffer per la riga di chat da mostrare localmente.
    sprintf(me, "IO: %s\n", txt); // Aggiunge l'etichetta "IO" e un ritorno a capo al messaggio.
    gtk_text_buffer_insert(buf_chat, &iter, me, -1); // Inserisce il messaggio nel buffer della chat.
    gtk_entry_set_text(e, ""); // Svuota il campo di testo dopo l'invio.
}

// --- AZIONI GIOCO ---
void on_crea(GtkWidget *w, gpointer d) { // Callback eseguita quando si preme il pulsante per creare una partita.
    PacchettoRete p; // Crea il pacchetto di richiesta.
    memset(&p, 0, sizeof(p)); // Azzera tutti i campi prima di impostare quelli necessari.
    p.tipo_messaggio=10; // Imposta il tipo di richiesta a 10, corrispondente alla creazione di una partita.
    send(client_socket,(char*)&p,sizeof(p),0); // Invia la richiesta al server.
}
void on_join(GtkWidget *w, gpointer d) { //È la callback collegata al pulsante Unisciti.
    PacchettoRete p; // Crea il pacchetto per chiedere di entrare in una partita.
    memset(&p, 0, sizeof(p)); // Azzera i campi del pacchetto.
    p.tipo_messaggio=11; // Specifica al server che si tratta di una richiesta di ingresso.
    p.id_partita = atoi(gtk_entry_get_text(GTK_ENTRY(entry_id))); // Converte in numero l'ID inserito dall'utente.
    send(client_socket,(char*)&p,sizeof(p),0); // Invia al server il tipo di richiesta e l'ID della partita.
}
void on_esc(GtkWidget *w, gpointer d) { //È chiamata per uscire dalla partita corrente.
    vittoria_fuochi = 0; // Spegne i fuochi d'artificio.
    PacchettoRete p; // Crea il pacchetto per segnalare l'uscita dalla partita.
    memset(&p, 0, sizeof(p)); // Azzera i campi del pacchetto.
    p.tipo_messaggio=16; // Identifica il messaggio come richiesta di uscita.
    send(client_socket, (char*)&p, sizeof(p), 0); // Comunica al server che il giocatore vuole uscire.
    for(int r = 0; r < 3; r++) {
        for(int c = 0; c < 3; c++) {
            gtk_button_set_image(GTK_BUTTON(griglia[r][c].b), NULL); // Toglie l'immagine dalla casella.
            board_locale[r][c] = ' '; // Svuota la casella nella griglia locale.
            GtkStyleContext *ctx = gtk_widget_get_style_context(griglia[r][c].b); // Recupera lo stile della casella.
            gtk_style_context_remove_class(ctx, "vincente"); // Toglie l'eventuale colore verde del tris.
        }
    }
    gtk_widget_set_sensitive(grid_g, FALSE); // Disabilita la griglia.
    gtk_label_set_text(GTK_LABEL(lbl_s), "In attesa..."); // Ripristina il testo di stato.
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "lobby"); // Torna alla pagina della lobby.
}

gboolean gestisci_rete(gpointer data) {//Questa funzione gestisce nel ciclo principale GTK i pacchetti ricevuti dal thread di rete.
    PacchettoRete *p = (PacchettoRete *)data; // Recupera il pacchetto ricevuto dal thread di rete.
    if (p->id_partita != -1) {// Controlla se il pacchetto contiene un ID di partita valido.
        id_p = p->id_partita; // Memorizza l'ID della partita comunicato dal server.
        char t[50]; // Prepara il testo del titolo della finestra.
        sprintf(t, "Tris Stellare - Stanza %d", id_p); // Inserisce l'ID nel titolo.
        gtk_window_set_title(GTK_WINDOW(win), t); // Aggiorna il titolo della finestra GTK.
    }

    if (p->tipo_messaggio == 26) { // Skin dell'avversario
        strcpy(avversario_skin, p->payload); // Memorizza il file della skin scelta dall'avversario.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }

    if (p->tipo_messaggio == 20) { // Lobby
        GtkTextIter it; // Dichiara l'iteratore per modificare il testo della lobby.
        gtk_text_buffer_get_end_iter(buf_lobby, &it); // Lo posiziona alla fine del testo esistente.
        gtk_text_buffer_insert(buf_lobby, &it, p->payload, -1); // Aggiunge al buffer il testo ricevuto.
        gtk_text_buffer_insert(buf_lobby, &it, "\n", -1); // Aggiunge un ritorno a capo dopo il messaggio.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }
    else if (p->tipo_messaggio == 21) { // Chat Gioco
        GtkTextIter it; // Dichiara l'iteratore per aggiungere testo alla chat.
        gtk_text_buffer_get_end_iter(buf_chat, &it); // Lo posiziona alla fine della chat.
        char him[300]; // Prepara una stringa per il messaggio dell'avversario.
        sprintf(him, "AVV: %s\n", p->payload); // Aggiunge l'etichetta "AVV" e il ritorno a capo.
        gtk_text_buffer_insert(buf_chat, &it, him, -1); // Visualizza il messaggio nel buffer della chat.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }
    else if (p->tipo_messaggio == 25) { // Messaggio di errore dal server
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, NULL); // Crea un dialogo informativo con il pulsante OK.
        gtk_window_set_decorated(GTK_WINDOW(d), FALSE); // Toglie la barra del titolo.
        gtk_widget_set_name(d, "nuvola-alert"); // Applica lo stile del dialogo giallo.
        char markup_testo[512]; // Prepara il testo formattato del dialogo.
        sprintf(markup_testo, "<span foreground='black' weight='bold' size='large'>%s</span>", p->payload); // Inserisce il messaggio del server in grassetto.
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(d), markup_testo); // Imposta il testo del dialogo.
        gtk_dialog_run(GTK_DIALOG(d)); // Mostra il dialogo e attende che venga chiuso.
        gtk_widget_destroy(d); // Libera il dialogo.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }
    else if (p->tipo_messaggio == 12) {// Sfida in arrivo
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, NULL); // Crea la finestra che chiede se accettare la sfida.
        gtk_window_set_decorated(GTK_WINDOW(d), FALSE); // Toglie la barra del titolo.
        gtk_widget_set_name(d, "nuvola-alert"); // Applica lo stile del dialogo giallo.
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(d), "<span foreground='black' weight='bold' size='large'>Sfida in arrivo! Accetti?</span>"); // Imposta la domanda in grassetto.
        int r = gtk_dialog_run(GTK_DIALOG(d)); // Mostra la finestra e attende la scelta dell'utente.
        gtk_widget_destroy(d); // Chiude e libera la finestra di dialogo.
        PacchettoRete resp; // Dichiara il pacchetto con cui rispondere alla sfida.
        memset(&resp, 0, sizeof(resp)); // Azzera i campi della risposta.
        resp.tipo_messaggio=13; // Indica al server che il pacchetto è una risposta alla sfida.
        resp.mossa_x=(r==GTK_RESPONSE_YES); // Memorizza 1 se accetta e 0 se rifiuta.
        send(client_socket,(char*)&resp,sizeof(resp),0); // Invia la risposta al server.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }

    if (p->tipo_messaggio == 14) { // Inizio Partita
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "gioco"); // Mostra la pagina della partita.
        gtk_text_buffer_set_text(buf_chat, "--- Inizio Chat ---\n", -1); // Inizializza il testo della chat.
    }
    else if (p->tipo_messaggio == 1) { // Mossa avversario
        imposta_img(griglia[p->mossa_x][p->mossa_y].b, avversario_skin); // Disegna nella casella la skin dell'avversario.
        board_locale[p->mossa_x][p->mossa_y] = 'A'; // Segna la casella come occupata dall'avversario.
        mio_t = 1; // Segna che, dopo la mossa avversaria, tocca a questo giocatore.
        gtk_widget_set_sensitive(grid_g, TRUE); // Riabilita la griglia per permettere una mossa.
    }
    else if (p->tipo_messaggio == 3 || p->tipo_messaggio == 4 || p->tipo_messaggio == 5) {// Messaggi di fine partita: vittoria, sconfitta o pareggio.
        if (p->tipo_messaggio == 3) { // Vittoria
            vittoria_fuochi = 1; // Accende i fuochi d'artificio.
            for (int i = 0; i < NUM_FUOCHI; i++) fuochi[i].active = 0; // Libera tutte le particelle per ripartire da zero.
        }
        if (p->tipo_messaggio == 4) { // Sconfitta: il server comunica anche la mossa vincente dell'avversario.
            imposta_img(griglia[p->mossa_x][p->mossa_y].b, avversario_skin); // Disegna l'ultima mossa dell'avversario.
            board_locale[p->mossa_x][p->mossa_y] = 'A'; // La segna nella griglia locale.
        } else if (p->tipo_messaggio == 5) { // Pareggio: l'ultima mossa può essere propria o dell'avversario.
            if (gtk_button_get_image(GTK_BUTTON(griglia[p->mossa_x][p->mossa_y].b)) == NULL) { // Se la casella non ha ancora un'immagine, la mossa era dell'avversario.
                imposta_img(griglia[p->mossa_x][p->mossa_y].b, avversario_skin); // Disegna l'ultima mossa dell'avversario.
                board_locale[p->mossa_x][p->mossa_y] = 'A'; // La segna nella griglia locale.
            }
        }
        if (p->tipo_messaggio == 3 || p->tipo_messaggio == 4) trova_e_colora_tris(); // In caso di vittoria o sconfitta evidenzia il tris.

        gtk_widget_queue_draw(win); // Chiede di ridisegnare la finestra con l'ultima mossa e il tris colorato.
        while (gtk_events_pending()) gtk_main_iteration(); // Esegue subito il ridisegno prima di aprire il dialogo.

        dialogo_corrente = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, NULL); // Crea il dialogo di fine partita con Sì/No.
        gtk_window_set_decorated(GTK_WINDOW(dialogo_corrente), FALSE); // Toglie la barra del titolo.
        gtk_widget_set_name(dialogo_corrente, "nuvola-alert"); // Applica lo stile del dialogo giallo.
        char markup_testo[512]; // Prepara il testo formattato del dialogo.
        sprintf(markup_testo, "<span foreground='black' weight='bold' size='large'>%s\nVuoi giocare la rivincita?</span>", p->payload); // Mostra l'esito e chiede la rivincita.
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialogo_corrente), markup_testo); // Imposta il testo del dialogo.
        int r = gtk_dialog_run(GTK_DIALOG(dialogo_corrente)); // Mostra il dialogo e attende la risposta.

        if (dialogo_corrente) { // Il dialogo può essere già stato chiuso se l'avversario è uscito (tipo 24).
            gtk_widget_destroy(dialogo_corrente); // Chiude e libera il dialogo.
            dialogo_corrente = NULL; // Segna che non c'è più un dialogo aperto.
        }
        if (r == GTK_RESPONSE_YES) { // Il giocatore vuole la rivincita.
            PacchettoRete req; // Dichiara il pacchetto di richiesta rivincita.
            memset(&req, 0, sizeof(req)); // Azzera i campi del pacchetto.
            req.tipo_messaggio = 22; // Indica al server la richiesta di rivincita.
            send(client_socket, (char*)&req, sizeof(req), 0); // Invia la richiesta al server.
            gtk_label_set_text(GTK_LABEL(lbl_s), "In attesa di conferma..."); // Avvisa che si aspetta l'avversario.
        } else if (r == GTK_RESPONSE_NO || r == GTK_RESPONSE_DELETE_EVENT) { // Il giocatore rifiuta o chiude il dialogo.
            on_esc(NULL, NULL); // Esce dalla partita e torna alla lobby.
        }
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }
    else if (p->tipo_messaggio == 23) { // Rivincita accettata da entrambi
        vittoria_fuochi = 0; // Spegne i fuochi d'artificio.
        for(int r = 0; r < 3; r++) {
            for(int c = 0; c < 3; c++) {
                gtk_button_set_image(GTK_BUTTON(griglia[r][c].b), NULL); // Toglie l'immagine dalla casella.
                board_locale[r][c] = ' '; // Svuota la casella nella griglia locale.
                GtkStyleContext *ctx = gtk_widget_get_style_context(griglia[r][c].b); // Recupera lo stile della casella.
                gtk_style_context_remove_class(ctx, "vincente"); // Toglie l'eventuale colore verde del tris.
            }
        }
        if (mio_t == 0) { // Nella rivincita i turni si invertono rispetto alla fine della partita.
            mio_t = 1; // Ora tocca a questo giocatore.
            gtk_widget_set_sensitive(grid_g, TRUE); // Abilita la griglia.
        } else {
            mio_t = 0; // Ora tocca all'avversario.
            gtk_widget_set_sensitive(grid_g, FALSE); // Disabilita la griglia.
        }
        gtk_label_set_text(GTK_LABEL(lbl_s), p->payload); // Mostra il messaggio del server.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }
    else if (p->tipo_messaggio == 24) { // L'avversario ha abbandonato: si resta nella stanza in attesa di un nuovo sfidante.
        vittoria_fuochi = 0; // Spegne i fuochi d'artificio.
        if (dialogo_corrente) { // Se era aperto il dialogo della rivincita...
            gtk_widget_destroy(dialogo_corrente); // ...lo chiude...
            dialogo_corrente = NULL; // ...e segna che non c'è più.
        }
        for(int r = 0; r < 3; r++) {
            for(int c = 0; c < 3; c++) {
                gtk_button_set_image(GTK_BUTTON(griglia[r][c].b), NULL); // Toglie l'immagine dalla casella.
                board_locale[r][c] = ' '; // Svuota la casella nella griglia locale.
                GtkStyleContext *ctx = gtk_widget_get_style_context(griglia[r][c].b); // Recupera lo stile della casella.
                gtk_style_context_remove_class(ctx, "vincente"); // Toglie l'eventuale colore verde del tris.
            }
        }
        mio_t = 0; // Non è il turno di nessuno finché non arriva un nuovo sfidante.
        gtk_widget_set_sensitive(grid_g, FALSE); // Disabilita la griglia.
        gtk_label_set_text(GTK_LABEL(lbl_s), "In attesa..."); // Ripristina il testo di stato.

        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, NULL); // Crea un dialogo informativo.
        gtk_window_set_decorated(GTK_WINDOW(d), FALSE); // Toglie la barra del titolo.
        gtk_widget_set_name(d, "nuvola-alert"); // Applica lo stile del dialogo giallo.
        char markup_testo[512]; // Prepara il testo formattato del dialogo.
        sprintf(markup_testo, "<span foreground='black' weight='bold' size='large'>%s</span>", p->payload); // Inserisce il messaggio del server in grassetto.
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(d), markup_testo); // Imposta il testo del dialogo.
        gtk_dialog_run(GTK_DIALOG(d)); // Mostra il dialogo e attende che venga chiuso.
        gtk_widget_destroy(d); // Libera il dialogo.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }
    else if (p->tipo_messaggio == 2 && strstr(p->payload, "lasciato")) { // L'avversario ha lasciato la partita.
        vittoria_fuochi = 0; // Spegne i fuochi d'artificio.
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, NULL); // Crea un dialogo di avviso.
        gtk_window_set_decorated(GTK_WINDOW(d), FALSE); // Toglie la barra del titolo.
        gtk_widget_set_name(d, "nuvola-alert"); // Applica lo stile del dialogo giallo.
        char markup_testo[512]; // Prepara il testo formattato del dialogo.
        sprintf(markup_testo, "<span foreground='black' weight='bold' size='large'>%s\nPartita interrotta.</span>", p->payload); // Inserisce il messaggio del server.
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(d), markup_testo); // Imposta il testo del dialogo.
        gtk_dialog_run(GTK_DIALOG(d)); // Mostra il dialogo e attende che venga chiuso.
        gtk_widget_destroy(d); // Libera il dialogo.
        on_esc(NULL, NULL); // Esce dalla partita e torna alla lobby.
        free(p); // Libera la memoria del pacchetto.
        return FALSE; // Termina la gestione del pacchetto.
    }

    if (strstr(p->payload, "1° Turno") || strstr(p->payload, "Tocca a te")) { // Il server indica che tocca a questo giocatore.
        mio_t=1; // Segna che il giocatore può fare la sua mossa.
        gtk_widget_set_sensitive(grid_g, TRUE); // Abilita la griglia.
    }
    else if (strstr(p->payload, "2° Turno")) { // Il server indica che il giocatore muove per secondo.
        mio_t=0; // Segna che per ora il turno non è del giocatore.
        gtk_widget_set_sensitive(grid_g, FALSE); // Disabilita la griglia finché non sarà il suo turno.
    }

    gtk_label_set_text(GTK_LABEL(lbl_s), p->payload); // Mostra nel testo di stato l'ultimo messaggio ricevuto.

    if ((p->tipo_messaggio == 2 && strstr(p->payload, "Iniziata!")) || (p->tipo_messaggio == 14 && strstr(p->payload, "Entrato!"))) { // A inizio partita (host o sfidante)...
        scegli_skin(); // ...fa scegliere l'avatar al giocatore.
    }

    free(p); // Libera la memoria allocata per il pacchetto nel thread di ascolto.
    return FALSE; // Indica a GTK che la callback non deve essere ripetuta.
}

void *thread_ascolto(void *arg) {//Funzione eseguita in un thread separato. Riceve continuamente i dati dal server.
    while (1) {
        PacchettoRete *p = malloc(sizeof(PacchettoRete)); // Riserva memoria per il prossimo pacchetto ricevuto.
        int bytes_ricevuti = 0; // Conta i byte del pacchetto ricevuti finora.
        while (bytes_ricevuti < sizeof(PacchettoRete)) { // TCP può spezzare il pacchetto: continua finché non è completo.
            int r = recv(client_socket, ((char*)p) + bytes_ricevuti, sizeof(PacchettoRete) - bytes_ricevuti, 0); // Riceve i byte mancanti.
            if (r <= 0) { // La connessione è terminata o la ricezione è fallita.
                free(p); // Libera il pacchetto incompleto.
                return NULL; // Termina il thread.
            }
            bytes_ricevuti += r; // Aggiorna il conteggio dei byte ricevuti.
        }
        g_idle_add(gestisci_rete, p); // Pianifica la gestione del pacchetto nel ciclo eventi di GTK.
    }
    return NULL; // Termina il thread senza restituire un risultato.
}

void on_click(GtkWidget *w, gpointer d) {// Callback eseguita quando l'utente clicca su una casella della griglia.
    if(!mio_t) return; // Ignora il clic se non è il turno del giocatore.
    DatiB *db = (DatiB*)d; // Recupera le coordinate della casella associate al pulsante premuto.
    PacchettoRete p; // Crea il pacchetto che rappresenta la mossa.
    memset(&p, 0, sizeof(p)); // Azzera i campi del pacchetto.
    p.tipo_messaggio=1; // Indica che il pacchetto contiene una mossa.
    p.mossa_x=db->r; // Inserisce nel pacchetto la riga della casella scelta.
    p.mossa_y=db->c; // Inserisce nel pacchetto la colonna della casella scelta.
    send(client_socket, (char*)&p, sizeof(p), 0); // Invia la mossa al server.
    imposta_img(w, mia_skin); // Mostra sulla casella la skin del giocatore.
    board_locale[db->r][db->c] = 'M'; // Segna la casella come occupata dal giocatore.
    mio_t = 0; // Segna che il turno è terminato.
    gtk_widget_set_sensitive(grid_g, FALSE); // Disabilita la griglia finché non arriva il turno successivo.
}

/*
La funzione main contiene:

l’avvio della rete;
la creazione della finestra;
la costruzione della lobby;
la costruzione della schermata di gioco;
l’avvio del ciclo di GTK.
Avvio e connessione Winsock
*/
int main(int argc, char *argv[]) {
#ifdef _WIN32              // Winsock va inizializzato solo su Windows; su macOS e Linux le socket sono pronte all'uso.
    WSADATA wsa; // Dichiara la struttura con le informazioni iniziali di Winsock.
    WSAStartup(MAKEWORD(2,2), &wsa); // Avvia Winsock nella versione 2.2.
#endif
    client_socket = socket(AF_INET, SOCK_STREAM, 0); // Crea un socket TCP per comunicare con il server.
    struct sockaddr_in a; // Dichiara la struttura con l'indirizzo IPv4 del server.
    a.sin_family=AF_INET; // Specifica che l'indirizzo usa il protocollo IPv4.
    a.sin_port=htons(PORT); // Imposta la porta del server convertendola nel formato di rete.
    a.sin_addr.s_addr=inet_addr("127.0.0.1"); // Imposta l'indirizzo del server sulla macchina locale.
    connect(client_socket, (struct sockaddr*)&a, sizeof(a)); // Avvia la connessione al server.
    pthread_t th; // Dichiara l'identificatore del thread di ascolto.
    pthread_create(&th, NULL, thread_ascolto, NULL); // Avvia in parallelo il thread che riceve i pacchetti.

    gtk_init(&argc, &argv); // Inizializza GTK e legge eventuali argomenti della libreria.
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL); // Crea la finestra principale dell'applicazione.
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 1000); // Imposta la dimensione iniziale della finestra.
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL); // Termina il ciclo GTK quando la finestra viene chiusa.

    GtkWidget *ov = gtk_overlay_new(); // Crea un contenitore che permette di sovrapporre widget.
    GtkWidget *da = gtk_drawing_area_new(); // Crea l'area su cui disegnare lo sfondo.
    g_signal_connect(da, "draw", G_CALLBACK(on_draw), NULL); // Collega il disegno dello sfondo alla callback on_draw.
    g_timeout_add(33, timer_sfondo, da); // Ridisegna lo sfondo ogni 33 ms per animare stelle, luna e fuochi.
    gtk_container_add(GTK_CONTAINER(ov), da); // Inserisce l'area di disegno nell'overlay.

    stack = gtk_stack_new(); // Crea il contenitore che alterna la pagina lobby e la pagina partita.
    gtk_overlay_add_overlay(GTK_OVERLAY(ov), stack); // Sovrappone lo stack allo sfondo.
    gtk_container_add(GTK_CONTAINER(win), ov); // Inserisce l'overlay nella finestra principale.

    // --- LOBBY PAGE ---
    GtkWidget *box_l = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20); // Crea il contenitore verticale della lobby con spaziatura 20.
    gtk_widget_set_halign(box_l, GTK_ALIGN_CENTER); // Centra la lobby orizzontalmente.
    gtk_widget_set_valign(box_l, GTK_ALIGN_CENTER); // Centra la lobby verticalmente.
    GtkWidget *btn_c = gtk_button_new_with_label("Crea Nuova Partita"); // Crea il pulsante per aprire una nuova partita.
    gtk_widget_set_name(btn_c, "btn-luna"); // Assegna al pulsante il nome usato dalle regole CSS.
    g_signal_connect(btn_c, "clicked", G_CALLBACK(on_crea), NULL); // Collega il clic alla richiesta di creazione partita.
    GtkWidget *box_j = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); // Crea il contenitore orizzontale per ID e pulsante.
    entry_id = gtk_entry_new(); // Crea il campo dove inserire l'ID della partita.
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_id), "ID"); // Mostra "ID" come testo guida nel campo.
    GtkWidget *btn_j = gtk_button_new_with_label("Unisciti"); // Crea il pulsante per entrare nella partita indicata.
    gtk_widget_set_name(btn_j, "btn-luna"); // Assegna al pulsante il nome usato dalle regole CSS.
    g_signal_connect(btn_j, "clicked", G_CALLBACK(on_join), NULL); // Collega il clic alla callback che invia la richiesta.
    gtk_box_pack_start(GTK_BOX(box_j), entry_id, TRUE, TRUE, 0); // Aggiunge il campo ID e gli consente di espandersi.
    gtk_box_pack_start(GTK_BOX(box_j), btn_j, FALSE, FALSE, 0); // Aggiunge il pulsante senza consentirgli di espandersi.

    GtkWidget *scr_l = gtk_scrolled_window_new(NULL, NULL); // Crea un riquadro con barre di scorrimento per la lobby.
    gtk_widget_set_size_request(scr_l, 450, 250); // Imposta la dimensione minima del riquadro.
    gtk_widget_set_margin_top(scr_l, 10); // Aggiunge un margine sopra il riquadro.
    GtkWidget *tv_l = gtk_text_view_new(); // Crea l'area di testo degli annunci.
    gtk_widget_set_name(tv_l, "chat-box"); // Assegna il nome usato dalle regole CSS dei riquadri di testo.
    buf_lobby = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_l)); // Recupera il buffer che contiene il testo degli annunci.
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv_l), FALSE); // Impedisce all'utente di modificare la bacheca.
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv_l), FALSE); // Nasconde il cursore di testo.
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv_l), 10); // Aggiunge un margine a sinistra del testo.
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv_l), 10); // Aggiunge un margine sopra il testo.
    gtk_container_add(GTK_CONTAINER(scr_l), tv_l); // Inserisce l'area di testo nel riquadro scorrevole.

    GtkWidget *lbl_t1 = gtk_label_new("LOBBY MULTIPLAYER"); // Crea il titolo della lobby.
    gtk_widget_set_name(lbl_t1, "testo-bianco"); // Assegna lo stile del testo bianco luminoso.
    GtkWidget *lbl_t2 = gtk_label_new("Bacheca Annunci:"); // Crea il titolo della bacheca.
    gtk_widget_set_name(lbl_t2, "testo-bianco"); // Assegna lo stile del testo bianco luminoso.
    gtk_box_pack_start(GTK_BOX(box_l), lbl_t1, 0,0,0); // Aggiunge il titolo della lobby.
    gtk_box_pack_start(GTK_BOX(box_l), btn_c, 0,0,0); // Aggiunge il pulsante per creare una partita.
    gtk_box_pack_start(GTK_BOX(box_l), box_j, 0,0,0); // Aggiunge il campo ID e il pulsante per unirsi.
    gtk_box_pack_start(GTK_BOX(box_l), lbl_t2, 0,0,0); // Aggiunge il titolo della bacheca.
    gtk_box_pack_start(GTK_BOX(box_l), scr_l, 1,1,0); // Aggiunge la bacheca, consentendole di occupare spazio extra.
    gtk_stack_add_named(GTK_STACK(stack), box_l, "lobby"); // Registra la pagina col nome "lobby" nello stack.

    // --- GAME PAGE ---
    GtkWidget *box_g_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30); // Crea il contenitore orizzontale principale della partita.
    gtk_widget_set_halign(box_g_main, GTK_ALIGN_CENTER); // Centra il contenuto della partita orizzontalmente.
    gtk_widget_set_valign(box_g_main, GTK_ALIGN_CENTER); // Centra il contenuto della partita verticalmente.

    // Sinistra: Griglia
    GtkWidget *box_grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15); // Crea il contenitore verticale della griglia e del suo stato.
    lbl_s = gtk_label_new("In attesa..."); // Crea l'etichetta che mostrerà lo stato della partita.
    gtk_widget_set_name(lbl_s, "testo-bianco"); // Assegna lo stile del testo bianco luminoso.
    grid_g = gtk_grid_new(); // Crea la griglia GTK che conterrà le nove caselle.
    gtk_grid_set_row_spacing(GTK_GRID(grid_g), 10); // Imposta lo spazio tra le righe della griglia.
    gtk_grid_set_column_spacing(GTK_GRID(grid_g), 10); // Imposta lo spazio tra le colonne della griglia.
    for(int r=0; r<3; r++) for(int c=0; c<3; c++) {
        griglia[r][c].r=r; // Salva nella struttura la riga della casella corrente.
        griglia[r][c].c=c; // Salva nella struttura la colonna della casella corrente.
        griglia[r][c].b = gtk_button_new(); // Crea il pulsante che rappresenta questa casella.
        gtk_widget_set_name(griglia[r][c].b, "grid-btn"); // Assegna lo stile CSS delle caselle.
        gtk_widget_set_size_request(griglia[r][c].b, 100, 100); // Imposta la dimensione minima del pulsante.
        g_signal_connect(griglia[r][c].b, "clicked", G_CALLBACK(on_click), &griglia[r][c]); // Collega il clic alla callback passando i dati della casella.
        gtk_grid_attach(GTK_GRID(grid_g), griglia[r][c].b, c, r, 1, 1); // Posiziona il pulsante nella colonna c e nella riga r.
    }
    GtkWidget *btn_e = gtk_button_new_with_label("Esci"); // Crea il pulsante per uscire dalla partita.
    gtk_widget_set_name(btn_e, "btn-luna"); // Assegna al pulsante lo stile giallo.
    g_signal_connect(btn_e, "clicked", G_CALLBACK(on_esc), NULL); // Collega il pulsante alla callback di uscita.
    gtk_box_pack_start(GTK_BOX(box_grid), lbl_s, 0,0,0); // Aggiunge l'etichetta di stato sopra la griglia.
    gtk_box_pack_start(GTK_BOX(box_grid), grid_g, 0,0,0); // Aggiunge la griglia sotto l'etichetta.
    gtk_box_pack_start(GTK_BOX(box_grid), btn_e, 0,0,0); // Aggiunge il pulsante di uscita sotto la griglia.

    // Destra: Chat
    GtkWidget *box_chat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); // Crea il contenitore verticale della chat.
    GtkWidget *scr_c = gtk_scrolled_window_new(NULL, NULL); // Crea un riquadro scorrevole per i messaggi.
    gtk_widget_set_size_request(scr_c, 250, 400); // Imposta la dimensione minima del riquadro della chat.
    GtkWidget *tv_c = gtk_text_view_new(); // Crea l'area in cui visualizzare i messaggi.
    gtk_widget_set_name(tv_c, "chat-box"); // Assegna il nome usato dalle regole CSS dei riquadri di testo.
    buf_chat = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_c)); // Recupera il buffer di testo della chat.
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv_c), FALSE); // Impedisce di modificare i messaggi già scritti.
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv_c), FALSE); // Nasconde il cursore di testo.
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv_c), 10); // Aggiunge un margine a sinistra del testo.
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv_c), 10); // Aggiunge un margine sopra il testo.
    gtk_container_add(GTK_CONTAINER(scr_c), tv_c); // Inserisce l'area di testo nel riquadro scorrevole.
    entry_chat = gtk_entry_new(); // Crea il campo in cui scrivere un messaggio.
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_chat), "Scrivi qui..."); // Mostra un suggerimento nel campo di testo.
    g_signal_connect(entry_chat, "activate", G_CALLBACK(invia_chat), NULL); // Invia il messaggio quando l'utente preme Invio.
    GtkWidget *lbl_t3 = gtk_label_new("CHAT PARTITA"); // Crea il titolo della chat.
    gtk_widget_set_name(lbl_t3, "testo-bianco"); // Assegna lo stile del testo bianco luminoso.
    gtk_box_pack_start(GTK_BOX(box_chat), lbl_t3, 0,0,0); // Aggiunge il titolo della chat.
    gtk_box_pack_start(GTK_BOX(box_chat), scr_c, 1,1,0); // Aggiunge l'area dei messaggi, che può espandersi.
    gtk_box_pack_start(GTK_BOX(box_chat), entry_chat, 0,0,0); // Aggiunge il campo di inserimento sotto i messaggi.

    gtk_box_pack_start(GTK_BOX(box_g_main), box_grid, 0,0,0); // Posiziona la griglia nella parte sinistra della pagina.
    gtk_box_pack_start(GTK_BOX(box_g_main), box_chat, 0,0,0); // Posiziona la chat accanto alla griglia.
    gtk_stack_add_named(GTK_STACK(stack), box_g_main, "gioco"); // Registra la pagina della partita con il nome "gioco".

    // CSS
    GtkCssProvider *cp = gtk_css_provider_new(); // Crea un provider per caricare le regole CSS dell'interfaccia.
    gtk_css_provider_load_from_data(cp, // Carica dal testo seguente le regole di stile dei widget.
        "#testo-bianco { color: #ffffff; font-weight: bold; font-size: 18px; text-shadow: 0px 0px 8px #00d9ff; }" // Titoli ed etichette bianchi con alone azzurro.
        "#btn-luna { background-image: none; background-color: #ffe64d; color: #000019; font-weight: bold; border-radius: 10px; border: 1px solid #ccb21c; padding: 6px; box-shadow: 0px 4px 6px rgba(0,0,0,0.5); }" // Pulsanti gialli arrotondati.
        "#btn-luna:hover { background-color: #ffef85; box-shadow: 0px 0px 12px #ffe64d; }" // Pulsanti gialli più chiari al passaggio del mouse.
        "#btn-luna label { color: #000019; font-size: 15px; }" // Testo scuro dentro i pulsanti gialli.
        "#grid-btn { background-image: none; background-color: rgba(255, 255, 255, 0.85); border: 3px solid rgba(0, 217, 255, 0.6); border-radius: 20px; }" // Caselle della griglia bianche con bordo azzurro.
        "#grid-btn:hover { background-color: rgba(255, 255, 255, 1.0); border-color: #00d9ff; box-shadow: 0px 0px 15px rgba(0, 217, 255, 0.8); }" // Caselle illuminate al passaggio del mouse.
        "button.vincente, button.vincente:disabled, button.vincente:backdrop, button.vincente:disabled:backdrop { background-image: none; background-color: #4dff4d; border: 3px solid #00cc00; color: black; box-shadow: 0px 0px 15px #4dff4d; }" // Caselle del tris vincente in verde, anche con griglia disabilitata.
        "#chat-box { background-color: rgba(10, 15, 45, 0.7); border-radius: 10px; border: 1px solid rgba(255, 255, 255, 0.2); }" // Riquadri di bacheca e chat blu semitrasparenti.
        "#chat-box text { background-color: transparent; color: #c4d4ff; font-family: monospace; font-size: 14px; }" // Testo azzurrino a larghezza fissa nei riquadri.
        "entry { background-color: rgba(255,255,255,0.05); color: #ffffff; border: 1px solid rgba(255,255,255,0.3); border-radius: 8px; padding: 5px 10px; }" // Campi di testo trasparenti con bordo chiaro.
        "entry:focus { border-color: #ffe64d; background-color: rgba(255,255,255,0.1); }" // Bordo giallo sul campo selezionato.
        "#nuvola-alert { background-color: #ffe64d; border-radius: 40px; border: 4px solid #ffffff; }" // Dialoghi gialli molto arrotondati a forma di nuvola.
        "#nuvola-alert label { color: #000000; font-family: sans-serif; }" // Testo nero nei dialoghi.
        "#nuvola-alert button { background-image: none; background-color: #ffffff; border-radius: 15px; border: 2px solid #000000; font-weight: bold; padding: 8px 20px; }" // Pulsanti bianchi bordati di nero nei dialoghi.
        "#nuvola-alert button label { color: #000000; }" // Testo nero nei pulsanti dei dialoghi.
        "#nuvola-alert button:hover { background-color: #000000; }" // Pulsante nero al passaggio del mouse...
        "#nuvola-alert button:hover label { color: #ffe64d; }" // ...con il testo giallo.
        , -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(cp), 800); // Applica le regole CSS a tutti i widget dello schermo.

    gtk_widget_show_all(win); // Mostra la finestra e tutti i widget contenuti.
    gtk_main(); // Avvia il ciclo eventi di GTK e mantiene attiva l'interfaccia.
    return 0; // Termina il programma segnalando l'uscita corretta.
}
