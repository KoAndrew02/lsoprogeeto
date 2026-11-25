#include <gtk/gtk.h>       // Importa le funzioni GTK usate per creare la finestra e i widget.
#include <pthread.h>       // Permette di creare il thread che ascolta i messaggi del server.
#include <stdio.h>         // Fornisce funzioni di input/output formattato, come sprintf.
#include <stdlib.h>        // Fornisce funzioni generali, come malloc, free, atoi e rand.
#include <string.h>        // Fornisce funzioni per manipolare stringhe e memoria.
#include <unistd.h>        // Importa funzioni POSIX; viene incluso anche se qui non è usato direttamente.
#include <winsock2.h>      // Fornisce le API Windows per la comunicazione tramite socket.
#include <math.h>          // Fornisce costanti e funzioni matematiche, tra cui M_PI.
#include "../shared/shared.h" // Condivide porta e formato dei pacchetti di rete con client e server.

// Raggruppa le coordinate di una casella e il pulsante GTK che la rappresenta.
typedef struct {
    int r;                 // Memorizza la riga della casella nella griglia.
    int c;                 // Memorizza la colonna della casella nella griglia.
    GtkWidget *b;          // Punta al pulsante GTK associato alla casella.
} DatiB;

int client_socket;         // Socket usato per inviare e ricevere dati dal server.
int id_p = -1;             // ID della partita corrente; -1 indica che non è ancora noto.
char mio_s[10] = "";       // Simbolo assegnato al giocatore: stella oppure luna.
int mio_t = 0;             // Vale 1 quando è il turno del giocatore e 0 quando non lo è.

GtkWidget *win, *stack, *lbl_s, *grid_g, *entry_id, *entry_chat; // Puntatori ai widget GTK condivisi tra le callback.
GtkTextBuffer *buf_lobby, *buf_chat; // Buffer di testo usati per aggiornare lobby e chat.
DatiB griglia[3][3];       // Dati delle nove caselle del tris.

// --- DISEGNO SFONDO (STESSO PER TUTTO) ---
    /*
        GtkWidget *w: il widget che viene ridisegnato, ovvero l’GtkDrawingArea da.
        cairo_t *cr: il contesto Cairo usato per disegnare colori, cerchi e altre forme.
        gpointer d: dati aggiuntivi eventualmente associati al segnale
    */
gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d)  {
    int width = gtk_widget_get_allocated_width(w); // Legge la larghezza disponibile per disegnare lo sfondo.
    int height = gtk_widget_get_allocated_height(w); // Legge l'altezza disponibile per disegnare lo sfondo.
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.1); // Imposta un colore blu molto scuro.
    cairo_paint(cr); // Riempie tutta l'area con il colore impostato.
    srand(42); // Inizializza il generatore casuale con un seme fisso per avere stelle sempre uguali.
    for (int i = 0; i < 100; i++) {
        double x = rand() % width; // Sceglie la coordinata orizzontale casuale della stella.
        double y = rand() % height; // Sceglie la coordinata verticale casuale della stella.
        cairo_set_source_rgb(cr, 1.0, 1.0, (rand()%2)); // Imposta il colore bianco o giallo chiaro.
        cairo_arc(cr, x, y, (rand()%2)+1, 0, 2*M_PI); // Definisce un piccolo cerchio nella posizione scelta.
        cairo_fill(cr); // Riempie il cerchio per disegnare una stella.
    }
    cairo_set_source_rgb(cr, 1.0, 0.9, 0.3); // Imposta il colore giallo della luna.
    cairo_arc(cr, width-120, 120, 70, 0, 2*M_PI); // Disegna il cerchio grande che forma la luna.
    cairo_fill(cr); // Riempie il cerchio giallo.
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.1); // Ripristina il colore dello sfondo per creare la falce.
    cairo_arc(cr, width-150, 120, 65, 0, 2*M_PI); // Disegna un cerchio scuro sovrapposto al primo.
    cairo_fill(cr); // Copre parte del cerchio giallo e lascia visibile una falce.
    return FALSE; // Comunica a GTK che questa callback non richiede altri disegnatori predefiniti.
}

    /*
        La funzione serve a modificare il contenuto visibile di un pulsante della griglia,
         sostituendo il suo testo o contenuto vuoto con l’immagine di Stella o Luna.
    */
void imposta_img(GtkWidget *b, const char *path) {
    GdkPixbuf *p = gdk_pixbuf_new_from_file(path, NULL); // Carica da disco l'immagine indicata dal percorso.
    if (!p) return; // Interrompe la funzione se il caricamento dell'immagine non è riuscito.
    GdkPixbuf *s = gdk_pixbuf_scale_simple(p, 60, 60, GDK_INTERP_BILINEAR); // Ridimensiona l'immagine a 60x60 pixel.
    gtk_button_set_image(GTK_BUTTON(b), gtk_image_new_from_pixbuf(s)); // Mostra l'immagine ridimensionata nel pulsante.
    g_object_unref(p); // Rilascia il riferimento al pixbuf originale.
    g_object_unref(s); // Rilascia il riferimento al pixbuf ridimensionato.
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
    PacchettoRete p; // Crea il pacchetto per segnalare l'uscita dalla partita.
    memset(&p, 0, sizeof(p)); // Azzera i campi del pacchetto.
    p.tipo_messaggio=16; // Identifica il messaggio come richiesta di uscita.
    send(client_socket,(char*)&p,sizeof(p),0); // Comunica al server che il giocatore vuole uscire.
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
    if (p->tipo_messaggio == 20) { // Lobby
        GtkTextIter it; // Dichiara l'iteratore per modificare il testo della lobby.
        gtk_text_buffer_get_end_iter(buf_lobby, &it); // Lo posiziona alla fine del testo esistente.
        gtk_text_buffer_insert(buf_lobby, &it, p->payload, -1); // Aggiunge al buffer il testo ricevuto.
        gtk_text_buffer_insert(buf_lobby, &it, "\n", -1); // Aggiunge un ritorno a capo dopo il messaggio.
    }
    else if (p->tipo_messaggio == 21) { // Chat Gioco
        GtkTextIter it; // Dichiara l'iteratore per aggiungere testo alla chat.
        gtk_text_buffer_get_end_iter(buf_chat, &it); // Lo posiziona alla fine della chat.
        char him[300]; // Prepara una stringa per il messaggio dell'avversario.
        sprintf(him, "AVV: %s\n", p->payload); // Aggiunge l'etichetta "AVV" e il ritorno a capo.
        gtk_text_buffer_insert(buf_chat, &it, him, -1); // Visualizza il messaggio nel buffer della chat.
    }
    else if (p->tipo_messaggio == 14) { // Inizio Partita
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "gioco"); // Mostra la pagina della partita.
        gtk_text_buffer_set_text(buf_chat, "--- Inizio Chat ---\n", -1); // Inizializza il testo della chat.
    }
    else if (p->tipo_messaggio == 12) {// Sfida in arrivo
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Sfida in arrivo! Accetti?"); // Crea la finestra che chiede se accettare la sfida.
        int r = gtk_dialog_run(GTK_DIALOG(d)); // Mostra la finestra e attende la scelta dell'utente.
        gtk_widget_destroy(d); // Chiude e libera la finestra di dialogo.
        PacchettoRete resp; // Dichiara il pacchetto con cui rispondere alla sfida.
        memset(&resp, 0, sizeof(resp)); // Azzera i campi della risposta.
        resp.tipo_messaggio=13; // Indica al server che il pacchetto è una risposta alla sfida.
        resp.mossa_x=(r==GTK_RESPONSE_YES); // Memorizza 1 se accetta e 0 se rifiuta.
        send(client_socket,(char*)&resp,sizeof(resp),0); // Invia la risposta al server.
    }
    else if (p->tipo_messaggio == 1) { // Mossa avversario
        imposta_img(griglia[p->mossa_x][p->mossa_y].b, (strcmp(mio_s, "★")==0)?"luna.png":"stella.png"); // Disegna nella casella il simbolo dell'avversario.
        mio_t = 1; // Segna che, dopo la mossa avversaria, tocca a questo giocatore.
        gtk_widget_set_sensitive(grid_g, TRUE); // Riabilita la griglia per permettere una mossa.
    }
    else if (p->tipo_messaggio == 3 || p->tipo_messaggio == 4 || p->tipo_messaggio == 5) {// Messaggi di fine partita: vittoria, sconfitta o pareggio.
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, p->payload); // Crea un dialogo informativo con il testo inviato dal server.
        gtk_dialog_run(GTK_DIALOG(d)); // Mostra il dialogo e attende che venga chiuso.
        gtk_widget_destroy(d); // Libera il dialogo dopo la visualizzazione.
        on_esc(NULL, NULL); // Esce dalla partita e riporta l'interfaccia alla lobby.
    }

    if (strstr(p->payload, "Stella")) { // Controlla se il server ha assegnato al giocatore il simbolo Stella.
        strcpy(mio_s, "★"); // Memorizza il simbolo Stella come simbolo personale.
        mio_t=1; // Segna che il giocatore può iniziare il proprio turno.
        gtk_widget_set_sensitive(grid_g, TRUE); // Abilita la griglia.
    }
    else if (strstr(p->payload, "Luna")) { // Controlla se il server ha assegnato al giocatore il simbolo Luna.
        strcpy(mio_s, "☾"); // Memorizza il simbolo Luna come simbolo personale.
        mio_t=0; // Segna che per ora il turno non è del giocatore.
        gtk_widget_set_sensitive(grid_g, FALSE); // Disabilita la griglia finché non sarà il suo turno.
    }
    
    gtk_label_set_text(GTK_LABEL(lbl_s), p->payload); // Mostra nel testo di stato l'ultimo messaggio ricevuto.
    free(p); // Libera la memoria allocata per il pacchetto nel thread di ascolto.
    return FALSE; // Indica a GTK che la callback non deve essere ripetuta.
}


void *thread_ascolto(void *arg) {//Funzione eseguita in un thread separato. Riceve continuamente i dati dal server.
    while (1) {
        PacchettoRete *p = malloc(sizeof(PacchettoRete)); // Riserva memoria per il prossimo pacchetto ricevuto.
        if (recv(client_socket, (char*)p, sizeof(PacchettoRete), 0) > 0) { // Attende un pacchetto dal server e verifica che sia arrivato.
            g_idle_add(gestisci_rete, p); // Pianifica la gestione del pacchetto nel ciclo eventi di GTK.
        }
        else break; // Interrompe il ciclo se la connessione è terminata o la ricezione è fallita.
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
    imposta_img(w, (strcmp(mio_s, "★")==0)?"stella.png":"luna.png"); // Mostra sulla casella il simbolo del giocatore.
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
    WSADATA wsa; // Dichiara la struttura con le informazioni iniziali di Winsock.
    WSAStartup(MAKEWORD(2,2), &wsa); // Avvia Winsock nella versione 2.2.
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
    gtk_widget_set_size_request(scr_l, 400, 200); // Imposta la dimensione minima del riquadro.
    GtkWidget *tv_l = gtk_text_view_new(); // Crea l'area di testo degli annunci.
    buf_lobby = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_l)); // Recupera il buffer che contiene il testo degli annunci.
    gtk_container_add(GTK_CONTAINER(scr_l), tv_l); // Inserisce l'area di testo nel riquadro scorrevole.

    gtk_box_pack_start(GTK_BOX(box_l), gtk_label_new("LOBBY MULTIPLAYER"), 0,0,0); // Aggiunge il titolo della lobby.
    gtk_box_pack_start(GTK_BOX(box_l), btn_c, 0,0,0); // Aggiunge il pulsante per creare una partita.
    gtk_box_pack_start(GTK_BOX(box_l), box_j, 0,0,0); // Aggiunge il campo ID e il pulsante per unirsi.
    gtk_box_pack_start(GTK_BOX(box_l), gtk_label_new("Bacheca Annunci:"), 0,0,0); // Aggiunge il titolo della bacheca.
    gtk_box_pack_start(GTK_BOX(box_l), scr_l, 1,1,0); // Aggiunge la bacheca, consentendole di occupare spazio extra.
    gtk_stack_add_named(GTK_STACK(stack), box_l, "lobby"); // Registra la pagina col nome "lobby" nello stack.

    // --- GAME PAGE ---
    GtkWidget *box_g_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30); // Crea il contenitore orizzontale principale della partita.
    gtk_widget_set_halign(box_g_main, GTK_ALIGN_CENTER); // Centra il contenuto della partita orizzontalmente.
    gtk_widget_set_valign(box_g_main, GTK_ALIGN_CENTER); // Centra il contenuto della partita verticalmente.

    // Sinistra: Griglia
    GtkWidget *box_grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15); // Crea il contenitore verticale della griglia e del suo stato.
    lbl_s = gtk_label_new("In attesa..."); // Crea l'etichetta che mostrerà lo stato della partita.
    grid_g = gtk_grid_new(); // Crea la griglia GTK che conterrà le nove caselle.
    gtk_grid_set_row_spacing(GTK_GRID(grid_g), 10); // Imposta lo spazio tra le righe della griglia.
    gtk_grid_set_column_spacing(GTK_GRID(grid_g), 10); // Imposta lo spazio tra le colonne della griglia.
    for(int r=0; r<3; r++) for(int c=0; c<3; c++) {
        griglia[r][c].r=r; // Salva nella struttura la riga della casella corrente.
        griglia[r][c].c=c; // Salva nella struttura la colonna della casella corrente.
        griglia[r][c].b = gtk_button_new(); // Crea il pulsante che rappresenta questa casella.
        gtk_widget_set_size_request(griglia[r][c].b, 100, 100); // Imposta la dimensione minima del pulsante.
        g_signal_connect(griglia[r][c].b, "clicked", G_CALLBACK(on_click), &griglia[r][c]); // Collega il clic alla callback passando i dati della casella.
        gtk_grid_attach(GTK_GRID(grid_g), griglia[r][c].b, c, r, 1, 1); // Posiziona il pulsante nella colonna c e nella riga r.
    }
    GtkWidget *btn_e = gtk_button_new_with_label("Esci"); // Crea il pulsante per uscire dalla partita.
    g_signal_connect(btn_e, "clicked", G_CALLBACK(on_esc), NULL); // Collega il pulsante alla callback di uscita.
    gtk_box_pack_start(GTK_BOX(box_grid), lbl_s, 0,0,0); // Aggiunge l'etichetta di stato sopra la griglia.
    gtk_box_pack_start(GTK_BOX(box_grid), grid_g, 0,0,0); // Aggiunge la griglia sotto l'etichetta.
    gtk_box_pack_start(GTK_BOX(box_grid), btn_e, 0,0,0); // Aggiunge il pulsante di uscita sotto la griglia.

    // Destra: Chat
    GtkWidget *box_chat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); // Crea il contenitore verticale della chat.
    GtkWidget *scr_c = gtk_scrolled_window_new(NULL, NULL); // Crea un riquadro scorrevole per i messaggi.
    gtk_widget_set_size_request(scr_c, 250, 400); // Imposta la dimensione minima del riquadro della chat.
    GtkWidget *tv_c = gtk_text_view_new(); // Crea l'area in cui visualizzare i messaggi.
    buf_chat = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_c)); // Recupera il buffer di testo della chat.
    gtk_container_add(GTK_CONTAINER(scr_c), tv_c); // Inserisce l'area di testo nel riquadro scorrevole.
    entry_chat = gtk_entry_new(); // Crea il campo in cui scrivere un messaggio.
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_chat), "Scrivi qui..."); // Mostra un suggerimento nel campo di testo.
    g_signal_connect(entry_chat, "activate", G_CALLBACK(invia_chat), NULL); // Invia il messaggio quando l'utente preme Invio.
    gtk_box_pack_start(GTK_BOX(box_chat), gtk_label_new("CHAT PARTITA"), 0,0,0); // Aggiunge il titolo della chat.
    gtk_box_pack_start(GTK_BOX(box_chat), scr_c, 1,1,0); // Aggiunge l'area dei messaggi, che può espandersi.
    gtk_box_pack_start(GTK_BOX(box_chat), entry_chat, 0,0,0); // Aggiunge il campo di inserimento sotto i messaggi.

    gtk_box_pack_start(GTK_BOX(box_g_main), box_grid, 0,0,0); // Posiziona la griglia nella parte sinistra della pagina.
    gtk_box_pack_start(GTK_BOX(box_g_main), box_chat, 0,0,0); // Posiziona la chat accanto alla griglia.
    gtk_stack_add_named(GTK_STACK(stack), box_g_main, "gioco"); // Registra la pagina della partita con il nome "gioco".

    // CSS
    GtkCssProvider *cp = gtk_css_provider_new(); // Crea un provider per caricare le regole CSS dell'interfaccia.
    gtk_css_provider_load_from_data(cp, // Carica dal testo seguente le regole di stile dei widget.
        "label { color: white; font-weight: bold; font-size: 18px; }"
        "#btn-luna { background: #ffe64d; color: #000019; font-weight: bold; border-radius: 10px; }"
        "textview text { color: white; background: rgba(0,0,0,0.4); font-family: monospace; }"
        "entry { background: rgba(255,255,255,0.1); color: white; border: 1px solid white; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(cp), 800); // Applica le regole CSS a tutti i widget dello schermo.

    gtk_widget_show_all(win); // Mostra la finestra e tutti i widget contenuti.
    gtk_main(); // Avvia il ciclo eventi di GTK e mantiene attiva l'interfaccia.
    return 0; // Termina il programma segnalando l'uscita corretta.
}