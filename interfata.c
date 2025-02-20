#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

extern int errno;

int port;
int sd;
char* user;
char* pass;
char buf[1024], userkeep[101];
GtkBuilder *builder;
GtkWidget *menu;
GtkWidget *errorAutLabel, *userLabel;
GtkWidget *app;
GtkEntry *password;
GtkTextView *printTextView;
GtkEntry *tvcautaTren, *tvcautaRuta, *tvstatie, *tvsetDelay;
pthread_t receiveThread;

gboolean show_app_window() {
    int x, y;
    gtk_window_get_position(GTK_WINDOW(menu), &x, &y);
    gtk_widget_hide(menu);
    gtk_widget_show_all(app);
    gtk_window_move(GTK_WINDOW(app), x, y);
    gtk_label_set_text(GTK_LABEL(userLabel), userkeep);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(printTextView);
    gtk_text_buffer_set_text(buffer, "Bine ati venit! Va rog introduceti statia curenta.", -1);
    gtk_entry_set_text(tvcautaTren, "");
    gtk_entry_set_text(tvcautaRuta, "");
    gtk_entry_set_text(tvstatie, "");
    gtk_entry_set_text(tvsetDelay, "");
    return G_SOURCE_REMOVE;
}
gboolean show_menu_window() {
    int x, y;
    gtk_window_get_position(GTK_WINDOW(app), &x, &y);
    gtk_widget_hide(app);
    gtk_widget_show_all(menu);
    gtk_window_move(GTK_WINDOW(menu), x, y);
    gtk_label_set_text(GTK_LABEL(errorAutLabel), " ");
    gtk_entry_set_text(password, "");
    return G_SOURCE_REMOVE;
}
void showErrorAutLabel(const char *errorMessage) {
    gtk_label_set_text(GTK_LABEL(errorAutLabel), errorMessage);
}

void update_text_buffer(const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(printTextView);
    gtk_text_buffer_set_text(buffer, text, -1);
}

gboolean update_text_buffer_idle(gpointer user_data) {
    const char *text = (const char *)user_data;
    update_text_buffer(text);
    return G_SOURCE_REMOVE;
}

void *receiveMessages(void *arg) {
    while (1) {
        if (read(sd, buf, sizeof(buf)) < 0) {
            perror("[client]Eroare la read() de la server.\n");
            break;
        }
        if (gtk_widget_get_visible(app)){
            if (strncmp(buf, "Atentie!", 8) == 0) {
                GtkTextBuffer *buffer = gtk_text_view_get_buffer(printTextView);
                GtkTextIter start, end;
                gtk_text_buffer_get_start_iter(buffer, &start);
                gtk_text_buffer_get_end_iter(buffer, &end);
                char current_text[1024];
                gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
                snprintf(current_text, sizeof(current_text), "%s", gtk_text_buffer_get_text(buffer, &start, &end, FALSE));
                char notif[4096];
                snprintf(notif, sizeof(notif), "%s\n\n%s", buf, current_text);

                g_idle_add(update_text_buffer_idle, (gpointer)g_strdup(notif));
            }
            else {
                g_idle_add(update_text_buffer_idle, (gpointer)g_strdup(buf));
            }
        }
        
        if(strcmp(buf, "Eroare la logare.") == 0) g_idle_add((GSourceFunc)showErrorAutLabel, "Eroare la logare.");
        else if(strcmp(buf, "Deconectat.") == 0) g_idle_add((GSourceFunc)show_menu_window, NULL);
        else if(strcmp(buf, "Quitting...") == 0) break;
        else if(strcmp(buf, "Logat.") == 0 || strcmp(buf, "Inregistrat.") == 0) {
            g_idle_add((GSourceFunc)show_app_window, NULL);
        }
    }
    if (user != NULL) {
        free(user);
        user = NULL;
    }
    if (pass != NULL) {
        free(pass);
        pass = NULL;
    }
    return NULL;
}
void on_menu_destroy(GtkWidget *widget, gpointer data) {
    strcpy(buf, "quit");
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    close(sd);
    gtk_main_quit();
}
void on_app_destroy(GtkWidget *widget, gpointer data) {
    strcpy(buf, "quit");
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    close(sd);
    gtk_main_quit();
}
void on_username_activated(GtkWidget *widget, gpointer data){
    user = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
    strcpy(userkeep, user);
}

void on_password_activated(GtkWidget *widget, gpointer data){
    pass = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
}
void on_login_clicked(GtkButton *button, gpointer user_data) {
    const char *command = "login";
    snprintf(buf, sizeof(buf), "%s %s %s", command, user, pass);
    buf[strcspn(buf, "\n")] = '\0';
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
void on_signin_clicked(GtkButton *button, gpointer user_data) {
    const char *command = "sign-in";
    snprintf(buf, sizeof(buf), "%s %s %s", command, user, pass);
    buf[strcspn(buf, "\n")] = '\0';
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
void on_getMersTren_clicked(GtkButton *button, gpointer user_data) {
    if (write(sd, "getMersTren", sizeof("getMersTren")) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
void on_getSosiri_clicked(GtkButton *button, gpointer user_data) {
    if (write(sd, "getSosiri", sizeof("getSosiri")) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
void on_getPlecari_clicked(GtkButton *button, gpointer user_data) {
    if (write(sd, "getPlecari", sizeof("getPlecari")) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
void on_trenActiv_clicked(GtkButton *button, gpointer user_data) {
    if (write(sd, "trenActiv", sizeof("trenActiv")) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
void on_statie_activated(GtkWidget *widget, gpointer data){
    char *st = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
    snprintf(buf, sizeof(buf), "statie %s", st);
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    free(st);
}
void on_statii_clicked(GtkWidget *widget, gpointer data){
    if (write(sd, "statii", sizeof("statii")) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
void on_cautaTren_activated(GtkWidget *widget, gpointer data){
    char *tr = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
    snprintf(buf, sizeof(buf), "cautaTren %s", tr);
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    gtk_entry_set_text(tvcautaTren, "");
    free(tr);
}
void on_cautaRuta_activated(GtkWidget *widget, gpointer data){
    char *ruta = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
    snprintf(buf, sizeof(buf), "cautaRuta %s", ruta);
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    gtk_entry_set_text(tvcautaRuta, "");
    free(ruta);
}
void on_setDelay_activated(GtkWidget *widget, gpointer data){
    char *delayinfo = strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
    snprintf(buf, sizeof(buf), "setDelay %s", delayinfo);
    if (write(sd, buf, sizeof(buf)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
    gtk_entry_set_text(tvsetDelay, "");
    free(delayinfo);
}
void on_logout_clicked(GtkWidget *widget, gpointer data){
    if (write(sd, "logout", sizeof("logout")) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
    }
}
int main(int argc, char *argv[]) {
    struct sockaddr_in server;

    if (argc != 3) {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }
    port = atoi(argv[2]);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket().\n");
        return errno;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    gtk_init(NULL, NULL);
    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "interfata.glade", NULL);
    if (pthread_create(&receiveThread, NULL, receiveMessages, NULL) != 0) {
        perror("[client]Error creating receive thread.\n");
        return errno;
    }
    menu = GTK_WIDGET(gtk_builder_get_object(builder, "menu"));
    g_signal_connect(menu, "destroy", G_CALLBACK(on_menu_destroy), NULL);
    GtkEntry *username; username = GTK_ENTRY(gtk_builder_get_object(builder, "username"));
    password = GTK_ENTRY(gtk_builder_get_object(builder, "password"));
    g_signal_connect(username, "activate", G_CALLBACK(on_username_activated), NULL);
    g_signal_connect(password, "activate", G_CALLBACK(on_password_activated), NULL);
    errorAutLabel = GTK_WIDGET(gtk_builder_get_object(builder, "Error_aut"));
    g_signal_connect(gtk_builder_get_object(builder, "login"), "clicked", G_CALLBACK(on_login_clicked), NULL);
    g_signal_connect(gtk_builder_get_object(builder, "sign-in"), "clicked", G_CALLBACK(on_signin_clicked), NULL);
    gtk_widget_show_all(menu);
    app = GTK_WIDGET(gtk_builder_get_object(builder, "app"));
    g_signal_connect(app, "destroy", G_CALLBACK(on_app_destroy), NULL);
    g_signal_connect(gtk_builder_get_object(builder, "getMersTren"), "clicked", G_CALLBACK(on_getMersTren_clicked), NULL);
    g_signal_connect(gtk_builder_get_object(builder, "getSosiri"), "clicked", G_CALLBACK(on_getSosiri_clicked), NULL);
    g_signal_connect(gtk_builder_get_object(builder, "getPlecari"), "clicked", G_CALLBACK(on_getPlecari_clicked), NULL);
    g_signal_connect(gtk_builder_get_object(builder, "trenActiv"), "clicked", G_CALLBACK(on_trenActiv_clicked), NULL);
    g_signal_connect(gtk_builder_get_object(builder, "statii"), "clicked", G_CALLBACK(on_statii_clicked), NULL);
    tvcautaTren = GTK_ENTRY(gtk_builder_get_object(builder, "cautaTren"));
    g_signal_connect(tvcautaTren, "activate", G_CALLBACK(on_cautaTren_activated), NULL);
    tvcautaRuta = GTK_ENTRY(gtk_builder_get_object(builder, "cautaRuta"));
    g_signal_connect(tvcautaRuta, "activate", G_CALLBACK(on_cautaRuta_activated), NULL);
    tvsetDelay = GTK_ENTRY(gtk_builder_get_object(builder, "setDelay"));
    g_signal_connect(tvsetDelay, "activate", G_CALLBACK(on_setDelay_activated), NULL);
    userLabel = GTK_WIDGET(gtk_builder_get_object(builder, "userLabel"));
    tvstatie = GTK_ENTRY(gtk_builder_get_object(builder, "statie"));
    g_signal_connect(tvstatie, "activate", G_CALLBACK(on_statie_activated), NULL);
    g_signal_connect(gtk_builder_get_object(builder, "logout"), "clicked", G_CALLBACK(on_logout_clicked), NULL);
    printTextView = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "print"));
    gtk_main();
    
    if (pthread_join(receiveThread, NULL) != 0) {
        perror("[client]Error joining receive thread.\n");
        return errno;
    }

    close(sd);
    return 0;
}

