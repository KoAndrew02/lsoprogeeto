#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <winsock2.h> 
#include "shared.h"

typedef struct {
    int riga;
    int colonna;
    GtkWidget *button;
} DatiBottone;

int client_socket;
GtkWidget *label_stato;
GtkWidget *grid_gioco;
GtkWidget *main_window;
GtkWidget *btn_rigioca; // Nuovo tasto per la rivincita
GtkWidget *btn_esci;    // Nuovo tasto per uscire

DatiBottone griglia_gui[3][3];
char mio_simbolo[10] = ""; 
int mio_turno = 0;

void pulisci_griglia_gui() {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            gtk_button_set_image(GTK_BUTTON(griglia_gui[r][c].button), NULL);
            gtk_button_set_label(GTK_BUTTON(griglia_gui[r][c].button), "");
            gtk_widget_set_sensitive(griglia_gui[r][c].button, TRUE);
        }
    }
}

void imposta_immagine_bottone(GtkWidget *bottone, const char *percorso_file) {
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(percorso_file, &error);
    if (!pixbuf) return;
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, 60, 60, GDK_INTERP_BILINEAR);
    GtkWidget *image = gtk_image_new_from_pixbuf(scaled);
    gtk_button_set_label(GTK_BUTTON(bottone), NULL);
    gtk_button_set_image(GTK_BUTTON(bottone), image);
    gtk_button_set_always_show_image(GTK_BUTTON(bottone), TRUE);
    g_object_unref(pixbuf); g_object_unref(scaled);
}

// --- AZIONI DEI NUOVI BOTTONI ---
void on_btn_rigioca_clicked(GtkWidget *widget, gpointer data) {
    PacchettoRete req;
    memset(&req, 0, sizeof(PacchettoRete));
    req.tipo_messaggio = 6; // Dico al server che voglio rigiocare
    send(client_socket, (char*)&req, sizeof(PacchettoRete), 0);
    
    gtk_widget_set_sensitive(btn_rigioca, FALSE); // Disattivo il tasto dopo averlo premuto
}

void on_btn_esci_clicked(GtkWidget *widget, gpointer data) {
    gtk_main_quit(); // Chiude il gioco correttamente
}
// --------------------------------

gboolean gestisci_messaggio_server(gpointer data) {
    PacchettoRete *p = (PacchettoRete *)data;

    // Aggiornamento della mossa sulla griglia
    if (p->tipo_messaggio == 1 || p->tipo_messaggio == 3 || p->tipo_messaggio == 4 || p->tipo_messaggio == 5) { 
        if (p->mossa_x != -1) {
            const char *file_avv = (strcmp(mio_simbolo, "★") == 0) ? "luna.png" : "stella.png";
            if (p->tipo_messaggio != 3) { 
                imposta_immagine_bottone(griglia_gui[p->mossa_x][p->mossa_y].button, file_avv);
                gtk_widget_set_sensitive(griglia_gui[p->mossa_x][p->mossa_y].button, FALSE);
            }
        }
        mio_turno = 1;
        gtk_widget_set_sensitive(grid_gioco, TRUE);
    } 

    // Messaggi di stato dal Server
    if (p->tipo_messaggio == 2) { 
        if (strstr(p->payload, "Stella (★)")) { strcpy(mio_simbolo, "★"); mio_turno = 1; gtk_widget_set_sensitive(grid_gioco, TRUE); } 
        else if (strstr(p->payload, "Luna (☾)")) { strcpy(mio_simbolo, "☾"); mio_turno = 0; gtk_widget_set_sensitive(grid_gioco, FALSE); }
        if (strstr(p->payload, "tocca a te")) { mio_turno = 1; gtk_widget_set_sensitive(grid_gioco, TRUE); }
    }
    else if (p->tipo_messaggio == 8) {
        // Il server ordina di pulire la griglia per la nuova partita
        pulisci_griglia_gui();
        gtk_widget_set_sensitive(grid_gioco, FALSE);
    }

    // --- GESTIONE FINE PARTITA NELLA GUI ---
    if (p->tipo_messaggio == 3) { // Vittoria
        gtk_label_set_text(GTK_LABEL(label_stato), "Hai Vinto! Clicca su Crea Partita.");
        gtk_widget_set_sensitive(grid_gioco, FALSE);
        gtk_button_set_label(GTK_BUTTON(btn_rigioca), "Crea Partita");
        gtk_widget_set_sensitive(btn_rigioca, TRUE);
    } 
    else if (p->tipo_messaggio == 4) { // Sconfitta
        gtk_label_set_text(GTK_LABEL(label_stato), "Hai Perso! Devi cliccare su Esci.");
        gtk_widget_set_sensitive(grid_gioco, FALSE);
        gtk_widget_set_sensitive(btn_rigioca, FALSE); // Il perdente NON può rigiocare
    } 
    else if (p->tipo_messaggio == 5) { // Pareggio
        gtk_label_set_text(GTK_LABEL(label_stato), "Pareggio! Clicca su Rivincita.");
        gtk_widget_set_sensitive(grid_gioco, FALSE);
        gtk_button_set_label(GTK_BUTTON(btn_rigioca), "Rivincita");
        gtk_widget_set_sensitive(btn_rigioca, TRUE);
    } 
    else {
        // Messaggio normale
        gtk_label_set_text(GTK_LABEL(label_stato), p->payload);
    }

    free(p);
    return G_SOURCE_REMOVE;
}

void *ascolta_server(void *arg) {
    while (1) {
        PacchettoRete *p = malloc(sizeof(PacchettoRete));
        memset(p, 0, sizeof(PacchettoRete));
        if (recv(client_socket, (char*)p, sizeof(PacchettoRete), 0) > 0) {
            g_idle_add(gestisci_messaggio_server, p);
        } else {
            free(p); break;
        }
    }
    return NULL;
}

void on_bottone_cliccato(GtkWidget *widget, gpointer data) {
    if (!mio_turno) return;
    DatiBottone *dati = (DatiBottone*)data;
    PacchettoRete mossa;
    memset(&mossa, 0, sizeof(PacchettoRete));
    mossa.tipo_messaggio = 1;
    mossa.mossa_x = dati->riga;
    mossa.mossa_y = dati->colonna;
    send(client_socket, (char*)&mossa, sizeof(PacchettoRete), 0);
    
    const char *mio_file = (strcmp(mio_simbolo, "★") == 0) ? "stella.png" : "luna.png";
    imposta_immagine_bottone(widget, mio_file);
    gtk_widget_set_sensitive(widget, FALSE);
    mio_turno = 0;
    gtk_widget_set_sensitive(grid_gioco, FALSE);
    gtk_label_set_text(GTK_LABEL(label_stato), "Attesa avversario...");
}

int main(int argc, char *argv[]) {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) return 1;

    pthread_t thread_rete;
    pthread_create(&thread_rete, NULL, ascolta_server, NULL);
    pthread_detach(thread_rete);

    gtk_init(&argc, &argv);
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Tris Stellare");
    gtk_container_set_border_width(GTK_CONTAINER(main_window), 15);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    
    label_stato = gtk_label_new("In connessione...");
    gtk_box_pack_start(GTK_BOX(vbox), label_stato, FALSE, FALSE, 5);
    
    grid_gioco = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_gioco), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid_gioco), 8);
    gtk_box_pack_start(GTK_BOX(vbox), grid_gioco, TRUE, TRUE, 5);

    // Creazione dei bottoni di gioco
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            griglia_gui[r][c].riga = r; griglia_gui[r][c].colonna = c;
            griglia_gui[r][c].button = gtk_button_new();
            gtk_widget_set_size_request(griglia_gui[r][c].button, 90, 90);
            g_signal_connect(griglia_gui[r][c].button, "clicked", G_CALLBACK(on_bottone_cliccato), &griglia_gui[r][c]);
            gtk_grid_attach(GTK_GRID(grid_gioco), griglia_gui[r][c].button, c, r, 1, 1);
        }
    }

    // --- NUOVA SEZIONE: Bottoni in basso ---
    GtkWidget *hbox_bottoni = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(hbox_bottoni), TRUE);

    btn_rigioca = gtk_button_new_with_label("Crea Partita");
    gtk_widget_set_sensitive(btn_rigioca, FALSE); // Spento all'inizio
    g_signal_connect(btn_rigioca, "clicked", G_CALLBACK(on_btn_rigioca_clicked), NULL);

    btn_esci = gtk_button_new_with_label("Esci");
    g_signal_connect(btn_esci, "clicked", G_CALLBACK(on_btn_esci_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(hbox_bottoni), btn_rigioca, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_bottoni), btn_esci, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_bottoni, FALSE, FALSE, 5);
    // ----------------------------------------

    gtk_widget_show_all(main_window);
    gtk_main();
    
    close(client_socket); 
    WSACleanup();
    return 0;
}