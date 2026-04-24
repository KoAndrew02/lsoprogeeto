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
DatiBottone griglia_gui[3][3];
char mio_simbolo = ' '; // Sarà 'X' o 'O'
int mio_turno = 0;      // 1 se posso cliccare, 0 altrimenti

// --- LOGICA DI RETE ---

gboolean gestisci_messaggio_server(gpointer data) {
    PacchettoRete *p = (PacchettoRete *)data;

    // Se il server ci manda una mossa dell'avversario (Tipo 1)
    if (p->tipo_messaggio == 1) {
        char simbolo_avversario = (mio_simbolo == 'X') ? 'O' : 'X';
        gtk_button_set_label(GTK_BUTTON(griglia_gui[p->mossa_x][p->mossa_y].button), (simbolo_avversario == 'X' ? "X" : "O"));
        gtk_widget_set_sensitive(griglia_gui[p->mossa_x][p->mossa_y].button, FALSE);
        mio_turno = 1; // Ora tocca a me
        gtk_widget_set_sensitive(grid_gioco, TRUE);
    } 
    // Se il server ci manda un aggiornamento di stato/assegnazione (Tipo 2)
    else if (p->tipo_messaggio == 2) {
        if (strstr(p->payload, "Sei il Giocatore X")) {
            mio_simbolo = 'X';
            mio_turno = 1;
            gtk_widget_set_sensitive(grid_gioco, TRUE);
        } else if (strstr(p->payload, "Tu sei O")) {
            mio_simbolo = 'O';
            mio_turno = 0;
            gtk_widget_set_sensitive(grid_gioco, FALSE);
        }
        
        // Se il messaggio dice che tocca a me, attivo la griglia
        if (strstr(p->payload, "tocca a te")) {
            mio_turno = 1;
            gtk_widget_set_sensitive(grid_gioco, TRUE);
        }
    }

    gtk_label_set_text(GTK_LABEL(label_stato), p->payload);
    free(p);
    return G_SOURCE_REMOVE;
}

void *ascolta_server(void *arg) {
    while (1) {
        PacchettoRete *p = malloc(sizeof(PacchettoRete));
        if (recv(client_socket, (char*)p, sizeof(PacchettoRete), 0) > 0) {
            g_idle_add(gestisci_messaggio_server, p);
        } else {
            free(p);
            break;
        }
    }
    return NULL;
}

// --- EVENTI GUI ---

void on_bottone_cliccato(GtkWidget *widget, gpointer data) {
    if (!mio_turno) return;

    DatiBottone *dati = (DatiBottone*)data;
    PacchettoRete mossa;
    mossa.tipo_messaggio = 1;
    mossa.mossa_x = dati->riga;
    mossa.mossa_y = dati->colonna;
    sprintf(mossa.payload, "Mossa in [%d,%d]", dati->riga, dati->colonna);

    send(client_socket, (char*)&mossa, sizeof(PacchettoRete), 0);

    gtk_button_set_label(GTK_BUTTON(widget), (mio_simbolo == 'X' ? "X" : "O"));
    gtk_widget_set_sensitive(widget, FALSE);
    
    mio_turno = 0; // Ho giocato, ora attendo
    gtk_widget_set_sensitive(grid_gioco, FALSE);
    gtk_label_set_text(GTK_LABEL(label_stato), "Attesa avversario...");
}

// --- MAIN ---

int main(int argc, char *argv[]) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

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

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Tris LSO");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    label_stato = gtk_label_new("Connessione...");
    gtk_box_pack_start(GTK_BOX(vbox), label_stato, FALSE, FALSE, 5);

    grid_gioco = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_gioco), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid_gioco), 5);
    gtk_box_pack_start(GTK_BOX(vbox), grid_gioco, TRUE, TRUE, 5);

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            griglia_gui[r][c].riga = r;
            griglia_gui[r][c].colonna = c;
            griglia_gui[r][c].button = gtk_button_new_with_label("");
            gtk_widget_set_size_request(griglia_gui[r][c].button, 80, 80);
            g_signal_connect(griglia_gui[r][c].button, "clicked", G_CALLBACK(on_bottone_cliccato), &griglia_gui[r][c]);
            gtk_grid_attach(GTK_GRID(grid_gioco), griglia_gui[r][c].button, c, r, 1, 1);
        }
    }

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}