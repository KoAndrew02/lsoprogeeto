#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <winsock2.h> 
#include <math.h> 
#include "../shared/shared.h"

typedef struct { int r; int c; GtkWidget *b; } DatiB;

int client_socket;
int id_p = -1;
char mio_s[10] = ""; 
int mio_t = 0;

GtkWidget *win, *stack, *lbl_s, *grid_g, *entry_id, *entry_chat;
GtkTextBuffer *buf_lobby, *buf_chat;
DatiB griglia[3][3];

// --- DISEGNO SFONDO (STESSO PER TUTTO) ---
gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
    int width = gtk_widget_get_allocated_width(w);
    int height = gtk_widget_get_allocated_height(w);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.1); cairo_paint(cr);
    srand(42);
    for (int i = 0; i < 100; i++) {
        double x = rand() % width, y = rand() % height;
        cairo_set_source_rgb(cr, 1.0, 1.0, (rand()%2));
        cairo_arc(cr, x, y, (rand()%2)+1, 0, 2*M_PI); cairo_fill(cr);
    }
    cairo_set_source_rgb(cr, 1.0, 0.9, 0.3);
    cairo_arc(cr, width-120, 120, 70, 0, 2*M_PI); cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.1);
    cairo_arc(cr, width-150, 120, 65, 0, 2*M_PI); cairo_fill(cr);
    return FALSE;
}

void imposta_img(GtkWidget *b, const char *path) {
    GdkPixbuf *p = gdk_pixbuf_new_from_file(path, NULL);
    if (!p) return;
    GdkPixbuf *s = gdk_pixbuf_scale_simple(p, 60, 60, GDK_INTERP_BILINEAR);
    gtk_button_set_image(GTK_BUTTON(b), gtk_image_new_from_pixbuf(s));
    g_object_unref(p); g_object_unref(s);
}

// --- AZIONI CHAT ---
void invia_chat(GtkEntry *e, gpointer d) {
    const char *txt = gtk_entry_get_text(e);
    if(strlen(txt) == 0) return;
    PacchettoRete p; memset(&p, 0, sizeof(p));
    p.tipo_messaggio = 21; strcpy(p.payload, txt);
    send(client_socket, (char*)&p, sizeof(p), 0);
    
    GtkTextIter iter; gtk_text_buffer_get_end_iter(buf_chat, &iter);
    char me[300]; sprintf(me, "IO: %s\n", txt);
    gtk_text_buffer_insert(buf_chat, &iter, me, -1);
    gtk_entry_set_text(e, "");
}

// --- AZIONI GIOCO ---
void on_crea(GtkWidget *w, gpointer d) { PacchettoRete p; memset(&p, 0, sizeof(p)); p.tipo_messaggio=10; send(client_socket,(char*)&p,sizeof(p),0); }
void on_join(GtkWidget *w, gpointer d) { 
    PacchettoRete p; memset(&p, 0, sizeof(p)); p.tipo_messaggio=11; 
    p.id_partita = atoi(gtk_entry_get_text(GTK_ENTRY(entry_id))); send(client_socket,(char*)&p,sizeof(p),0); 
}
void on_esc(GtkWidget *w, gpointer d) { 
    PacchettoRete p; memset(&p, 0, sizeof(p)); p.tipo_messaggio=16; send(client_socket,(char*)&p,sizeof(p),0);
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "lobby");
}

gboolean gestisci_rete(gpointer data) {
    PacchettoRete *p = (PacchettoRete *)data;
    if (p->id_partita != -1) {
        id_p = p->id_partita; char t[50]; sprintf(t, "Tris Stellare - Stanza %d", id_p);
        gtk_window_set_title(GTK_WINDOW(win), t);
    }
    if (p->tipo_messaggio == 20) { // Lobby
        GtkTextIter it; gtk_text_buffer_get_end_iter(buf_lobby, &it);
        gtk_text_buffer_insert(buf_lobby, &it, p->payload, -1); gtk_text_buffer_insert(buf_lobby, &it, "\n", -1);
    }
    else if (p->tipo_messaggio == 21) { // Chat Gioco
        GtkTextIter it; gtk_text_buffer_get_end_iter(buf_chat, &it);
        char him[300]; sprintf(him, "AVV: %s\n", p->payload);
        gtk_text_buffer_insert(buf_chat, &it, him, -1);
    }
    else if (p->tipo_messaggio == 14) { 
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "gioco");
        gtk_text_buffer_set_text(buf_chat, "--- Inizio Chat ---\n", -1);
    }
    else if (p->tipo_messaggio == 12) {
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Sfida in arrivo! Accetti?");
        int r = gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
        PacchettoRete resp; memset(&resp, 0, sizeof(resp)); resp.tipo_messaggio=13; resp.mossa_x=(r==GTK_RESPONSE_YES); send(client_socket,(char*)&resp,sizeof(resp),0);
    }
    else if (p->tipo_messaggio == 1) { // Mossa avversario
        imposta_img(griglia[p->mossa_x][p->mossa_y].b, (strcmp(mio_s, "★")==0)?"luna.png":"stella.png");
        mio_t = 1; gtk_widget_set_sensitive(grid_g, TRUE);
    }
    else if (p->tipo_messaggio == 3 || p->tipo_messaggio == 4 || p->tipo_messaggio == 5) {
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, p->payload);
        gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
        on_esc(NULL, NULL);
    }

    if (strstr(p->payload, "Stella")) { strcpy(mio_s, "★"); mio_t=1; gtk_widget_set_sensitive(grid_g, TRUE); }
    else if (strstr(p->payload, "Luna")) { strcpy(mio_s, "☾"); mio_t=0; gtk_widget_set_sensitive(grid_g, FALSE); }
    
    gtk_label_set_text(GTK_LABEL(lbl_s), p->payload);
    free(p); return FALSE;
}

void *thread_ascolto(void *arg) {
    while (1) {
        PacchettoRete *p = malloc(sizeof(PacchettoRete));
        if (recv(client_socket, (char*)p, sizeof(PacchettoRete), 0) > 0) g_idle_add(gestisci_rete, p);
        else break;
    }
    return NULL;
}

void on_click(GtkWidget *w, gpointer d) {
    if(!mio_t) return;
    DatiB *db = (DatiB*)d;
    PacchettoRete p; memset(&p, 0, sizeof(p)); p.tipo_messaggio=1; p.mossa_x=db->r; p.mossa_y=db->c;
    send(client_socket, (char*)&p, sizeof(p), 0);
    imposta_img(w, (strcmp(mio_s, "★")==0)?"stella.png":"luna.png");
    mio_t = 0; gtk_widget_set_sensitive(grid_g, FALSE);
}

int main(int argc, char *argv[]) {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a; a.sin_family=AF_INET; a.sin_port=htons(PORT); a.sin_addr.s_addr=inet_addr("127.0.0.1");
    connect(client_socket, (struct sockaddr*)&a, sizeof(a));
    pthread_t th; pthread_create(&th, NULL, thread_ascolto, NULL);

    gtk_init(&argc, &argv);
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 1000);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *ov = gtk_overlay_new();
    GtkWidget *da = gtk_drawing_area_new(); g_signal_connect(da, "draw", G_CALLBACK(on_draw), NULL);
    gtk_container_add(GTK_CONTAINER(ov), da);

    stack = gtk_stack_new();
    gtk_overlay_add_overlay(GTK_OVERLAY(ov), stack);
    gtk_container_add(GTK_CONTAINER(win), ov);

    // --- LOBBY PAGE ---
    GtkWidget *box_l = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(box_l, GTK_ALIGN_CENTER); gtk_widget_set_valign(box_l, GTK_ALIGN_CENTER);
    
    GtkWidget *btn_c = gtk_button_new_with_label("Crea Nuova Partita");
    gtk_widget_set_name(btn_c, "btn-luna");
    g_signal_connect(btn_c, "clicked", G_CALLBACK(on_crea), NULL);

    GtkWidget *box_j = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    entry_id = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry_id), "ID");
    GtkWidget *btn_j = gtk_button_new_with_label("Unisciti");
    gtk_widget_set_name(btn_j, "btn-luna");
    g_signal_connect(btn_j, "clicked", G_CALLBACK(on_join), NULL);
    gtk_box_pack_start(GTK_BOX(box_j), entry_id, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_j), btn_j, FALSE, FALSE, 0);

    GtkWidget *scr_l = gtk_scrolled_window_new(NULL, NULL); gtk_widget_set_size_request(scr_l, 400, 200);
    GtkWidget *tv_l = gtk_text_view_new(); buf_lobby = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_l));
    gtk_container_add(GTK_CONTAINER(scr_l), tv_l);

    gtk_box_pack_start(GTK_BOX(box_l), gtk_label_new("LOBBY MULTIPLAYER"), 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_l), btn_c, 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_l), box_j, 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_l), gtk_label_new("Bacheca Annunci:"), 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_l), scr_l, 1,1,0);
    gtk_stack_add_named(GTK_STACK(stack), box_l, "lobby");

    // --- GAME PAGE ---
    GtkWidget *box_g_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30);
    gtk_widget_set_halign(box_g_main, GTK_ALIGN_CENTER); gtk_widget_set_valign(box_g_main, GTK_ALIGN_CENTER);

    // Sinistra: Griglia
    GtkWidget *box_grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    lbl_s = gtk_label_new("In attesa...");
    grid_g = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid_g), 10); gtk_grid_set_column_spacing(GTK_GRID(grid_g), 10);
    for(int r=0; r<3; r++) for(int c=0; c<3; c++) {
        griglia[r][c].r=r; griglia[r][c].c=c;
        griglia[r][c].b = gtk_button_new(); gtk_widget_set_size_request(griglia[r][c].b, 100, 100);
        g_signal_connect(griglia[r][c].b, "clicked", G_CALLBACK(on_click), &griglia[r][c]);
        gtk_grid_attach(GTK_GRID(grid_g), griglia[r][c].b, c, r, 1, 1);
    }
    GtkWidget *btn_e = gtk_button_new_with_label("Esci"); g_signal_connect(btn_e, "clicked", G_CALLBACK(on_esc), NULL);
    gtk_box_pack_start(GTK_BOX(box_grid), lbl_s, 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_grid), grid_g, 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_grid), btn_e, 0,0,0);

    // Destra: Chat
    GtkWidget *box_chat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *scr_c = gtk_scrolled_window_new(NULL, NULL); gtk_widget_set_size_request(scr_c, 250, 400);
    GtkWidget *tv_c = gtk_text_view_new(); buf_chat = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_c));
    gtk_container_add(GTK_CONTAINER(scr_c), tv_c);
    entry_chat = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry_chat), "Scrivi qui...");
    g_signal_connect(entry_chat, "activate", G_CALLBACK(invia_chat), NULL);
    gtk_box_pack_start(GTK_BOX(box_chat), gtk_label_new("CHAT PARTITA"), 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_chat), scr_c, 1,1,0);
    gtk_box_pack_start(GTK_BOX(box_chat), entry_chat, 0,0,0);

    gtk_box_pack_start(GTK_BOX(box_g_main), box_grid, 0,0,0);
    gtk_box_pack_start(GTK_BOX(box_g_main), box_chat, 0,0,0);
    gtk_stack_add_named(GTK_STACK(stack), box_g_main, "gioco");

    // CSS
    GtkCssProvider *cp = gtk_css_provider_new();
    gtk_css_provider_load_from_data(cp,
        "label { color: white; font-weight: bold; font-size: 18px; }"
        "#btn-luna { background: #ffe64d; color: #000019; font-weight: bold; border-radius: 10px; }"
        "textview text { color: white; background: rgba(0,0,0,0.4); font-family: monospace; }"
        "entry { background: rgba(255,255,255,0.1); color: white; border: 1px solid white; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(cp), 800);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}