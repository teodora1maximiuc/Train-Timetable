#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sqlite3.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>

#define PORT 2908
#define MAX_CLIENTS 10
extern int errno;
time_t current_time;
struct tm *time_info;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thData{
    int idThread; //id-ul thread-ului tinut in evidenta de acest program
    int cl; //descriptorul intors de accept
    sqlite3 *db; 
    char username[50];
    char statie[50];
}thData;
typedef struct trSort{
    char traseu[1024];
    int time;
}trSort;

thData *activeClients[MAX_CLIENTS];
trSort *sortTrains[23];

void addClient(thData *td) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (activeClients[i] == NULL) {
            activeClients[i] = td;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}
void removeClient(thData *td) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (activeClients[i] == td) {
            activeClients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int ctoi(char time[6]);
int h_min(char time[10]);
void min_h(int min, char format[6]);
static void* checkTrains(void *arg);
static void *treat(void *);
unsigned int hashpswrd(const char *str);
int authentication(sqlite3 *db, const char *username, int hash);
void raspunde(thData *);
void comanda(char buf[1024], thData *td) {
    int ok = 0, timeT;
    xmlDocPtr doc;
    doc = xmlReadFile("trNr.xml", NULL, 0);
    if (doc == NULL)
        fprintf(stderr, "Failed to parse XML file.\n");
    xmlNodePtr root = xmlDocGetRootElement(doc);
    char username[50], password[50], insert[255], update[255], trainNr[11], s1[25],s2[25];
    int hash;
    if(strcmp(buf, "getMersTren") == 0) {
        if(strcmp(td->username, "") == 0) strcpy(buf, "Nu sunteti logat. Folositi <login>");
        else if(strcmp(td->statie, "") == 0) strcpy(buf, "Nu ati introdus statia curenta.");
        else{
            strcpy(buf, "");
            for(xmlNodePtr trains = root->children; trains!=NULL; trains = trains->next) {
                xmlNodePtr stations = xmlFirstElementChild(trains); ok = 0;
                xmlNodePtr delay = xmlNextElementSibling(stations);
                if(stations!=NULL && xmlStrEqual(stations->name, BAD_CAST "Stations")) {
                    for(xmlNodePtr station=stations->children; station!=NULL; station = station -> next) {
                        if(xmlStrEqual(station->name, BAD_CAST td->statie)) { ok= 1; break; }
                    }
                    if (ok == 1) {
                        strcat(buf, (char*)trains->name);
                        strcat(buf, " ");
                        xmlNodePtr st = xmlFirstElementChild(stations);
                        strcat(buf, (char*)st->name);
                        strcat(buf, "("); 
                        xmlChar* content = xmlNodeGetContent(st);
                        strcat(buf, (char*)content);
                        strcat(buf, ")"); xmlFree(content);
                        st = xmlNextElementSibling(st);
                        while (st != NULL) {
                            strcat(buf, " - ");
                            strcat(buf, (char*)st->name);
                            strcat(buf, "("); 
                            xmlChar* content = xmlNodeGetContent(st);
                            strcat(buf, (char*)content);
                            strcat(buf, ")"); xmlFree(content);
                            st = xmlNextElementSibling(st);
                        }
                        content = xmlNodeGetContent(delay);
                        strcat(buf, " Intarziere: "); 
                        strcat(buf, (char*)content); 
                        xmlFree(content);
                        strcat(buf, "\n");
                    }
                }
            }
        }
    }
    else if(strcmp(buf, "trenActiv") == 0) {
        if(strcmp(td->username, "") == 0) strcpy(buf, "Nu sunteti logat. Folositi <login>");
        else if(strcmp(td->statie, "") == 0) strcpy(buf, "Nu ati introdus statia curenta.");
        else{
        strcpy(buf, "Trenuri active: \n");
        for(xmlNodePtr trains =xmlFirstElementChild(root); trains!=NULL; trains = xmlNextElementSibling(trains)) {
            xmlNodePtr stations= xmlFirstElementChild(trains); 
            xmlNodePtr delay = xmlNextElementSibling(stations);
            xmlNodePtr status = xmlNextElementSibling(delay);
            xmlChar* content= xmlNodeGetContent(status);
            if(strcmp(content, "1") == 0) {
                xmlFree(content);
                strcat(buf, (char*)trains->name); strcat(buf, " ");
                xmlNodePtr st = xmlFirstElementChild(stations);
                strcat(buf, (char*)st->name); strcat(buf, "("); 
                xmlChar* content = xmlNodeGetContent(st); strcat(buf, (char*)content);
                strcat(buf, ")"); xmlFree(content);
                st = xmlNextElementSibling(st);
                while (st != NULL) {
                    strcat(buf, " - "); strcat(buf, (char*)st->name);
                    strcat(buf, "("); xmlChar* content = xmlNodeGetContent(st);
                    strcat(buf, (char*)content);
                    strcat(buf, ")"); xmlFree(content);
                    st = xmlNextElementSibling(st);
                }
                content = xmlNodeGetContent(delay);
                strcat(buf, " Intarziere: "); 
                strcat(buf, (char*)content); 
                xmlFree(content);
                strcat(buf, "\n");
            }
        }}
    }
    else if(strcmp(buf, "getSosiri") == 0 || strcmp(buf, "getPlecari") == 0) {
        if(strcmp(td->username, "") == 0) strcpy(buf, "Nu sunteti logat. Folositi <login>");
        else if(strcmp(td->statie, "") == 0) strcpy(buf, "Nu ati introdus statia curenta.");
        else{
        char info[1024],delayT[6]; int i = 0;
        for (int i = 0; i < 23; ++i) {
            sortTrains[i] = (trSort *)malloc(sizeof(trSort));
            if (sortTrains[i] == NULL) {
                fprintf(stderr, "Memory allocation error\n");
                exit(EXIT_FAILURE);
            }
            sortTrains[i]->time = INT_MAX; 
        }
        for(xmlNodePtr trains = root->children; trains!=NULL; trains = xmlNextElementSibling(trains)) {
            strcpy(info, "");
            xmlNodePtr stations = xmlFirstElementChild(trains); ok = 0;
            xmlNodePtr delay = xmlNextElementSibling(stations);
            if(stations!=NULL && xmlStrEqual(stations->name, BAD_CAST "Stations")) {
                if(strcmp(buf, "getSosiri") == 0) { // ----- - statie x
                    for(xmlNodePtr station=xmlFirstElementChild(stations); station!=NULL; station=xmlNextElementSibling(station)) {
                        if(xmlStrEqual(station->name, BAD_CAST td->statie)) { ok= 1; break; }
                    }
                    if (ok == 1) {
                        xmlNodePtr st = xmlFirstElementChild(stations);
                        if(!xmlStrEqual(st->name, BAD_CAST td->statie)) {
                            strcat(info, (char*)trains->name);
                            strcat(info, " ");
                            if (st != NULL) { 
                                strcat(info, " - "); strcat(info, (char*)st->name); strcat(info, "("); 
                                xmlChar *stationContent = xmlNodeGetContent(st);
                                strcat(info, (char*)stationContent);
                                strcat(info, ")");
                                st = xmlNextElementSibling(st);
                                while (st != NULL) {
                                    strcat(info, " - "); strcat(info, (char*)st->name); strcat(info, "("); 
                                    stationContent = xmlNodeGetContent(st);
                                    strcat(info, (char*)stationContent);
                                    strcat(info, ")");
                                    if (strcmp((char*)st->name, td->statie) == 0) {
                                        timeT = h_min((char *)stationContent); xmlFree(stationContent);
                                        break;
                                    }
                                    xmlFree(stationContent);
                                    st = xmlNextElementSibling(st);
                                }
                                xmlChar *content = xmlNodeGetContent(delay);
                                strcat(info, " Intarziere: "); 
                                strcat(info, (char*)content); 
                                xmlFree(content);
                                i++; strcpy(sortTrains[i]->traseu, info); 
                                sortTrains[i]->time = timeT;
                            }
                        }
                    }
                }
                else if (strcmp(buf, "getPlecari") == 0){ // x statie - -------
                    xmlNodePtr station=xmlFirstElementChild(stations);
                    while(xmlNextElementSibling(station) !=NULL){
                        if(xmlStrEqual(station->name, BAD_CAST td->statie)) {
                            xmlChar *stationContent = xmlNodeGetContent(station);
                            timeT = h_min((char *)stationContent); 
                            xmlFree(stationContent);
                            station = xmlNextElementSibling(station);
                            if(station==NULL) break;                        
                            ok= 1; break; }
                        station = xmlNextElementSibling(station);
                    }
                    if (ok == 1 && station->next!=NULL) {
                        strcat(info, (char*)trains->name); strcat(info, " ");
                        xmlNodePtr st = xmlFirstElementChild(stations);
                        while(st!=NULL && !xmlStrEqual(st->name, BAD_CAST td->statie)) { st= xmlNextElementSibling(st); }
                        while(st!=NULL) {
                            strcat(info, " - "); strcat(info, (char*)st->name); strcat(info, "("); 
                            xmlChar *stationContent = xmlNodeGetContent(st);
                            strcat(info, (char*)stationContent);
                            xmlFree(stationContent);
                            strcat(info, ")"); st = xmlNextElementSibling(st);
                        }
                        xmlChar *content = xmlNodeGetContent(delay);
                        strcat(info, " Intarziere: "); 
                        strcat(info, (char*)content); 
                        xmlFree(content);
                        i++; strcpy(sortTrains[i]->traseu, info); 
                        sortTrains[i]->time = timeT;
                    }
                }
            }
        }
        for (int i = 0; i < 23; ++i) {
            for (int j = i + 1; j < 23; ++j) {
                if (sortTrains[i]->time > sortTrains[j]->time) {
                    trSort *temp = sortTrains[i];
                    sortTrains[i] = sortTrains[j];
                    sortTrains[j] = temp;
                }
            }
        }
        current_time = time(NULL);
        time_info = localtime(&current_time);
        int hour = time_info->tm_hour;
        int min = time_info->tm_min;
        int minutes = 60*hour + min;
        strcpy(buf,""); ok = 0;
        for (int i = 0; i < 23; ++i) {
            int stt = sortTrains[i]->time;
            if ((stt != INT_MAX && stt >= minutes) && stt<=minutes + 60 ) {
                strcat(buf, sortTrains[i]->traseu); strcat(buf,"\n"); ok = 1;
            }
        }
        if(ok == 0) strcat(buf, "Niciun tren pentru urmatoarea ora.");
        for (int i = 0; i < 23; ++i) {
            free(sortTrains[i]);
        }}
    }
    else if(strncmp(buf, "setDelay", 8) == 0) {
        char dValue[10], name_Stations[101];
        strcpy(name_Stations, "");
        sscanf(buf, "setDelay %s %s", trainNr, dValue);
        for (xmlNodePtr trains = xmlFirstElementChild(root); trains != NULL; trains = xmlNextElementSibling(trains)) {
            if (xmlStrEqual(trains->name, BAD_CAST trainNr)) {
                xmlNodePtr stations = xmlFirstElementChild(trains);
                for (xmlNodePtr station = stations->children; station != NULL; station = xmlNextElementSibling(station)) {
                    strcat(name_Stations, (char *)station->name);
                    strcat(name_Stations, " ");
                }
                xmlNodePtr delay = xmlNextElementSibling(stations);
                xmlNodePtr status = xmlNextElementSibling(delay);
                xmlChar *content = xmlNodeGetContent(status);
                if(strcmp(content, "0") == 0) {
                    strcpy(buf, "Trenul nu este in mers."); break;
                }
                xmlFree(content);
                if (delay != NULL) {
                    xmlNodeSetContent(delay, BAD_CAST dValue);
                    xmlSaveFormatFile("trNr.xml", doc, 1);

                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; ++i) {
                        if (activeClients[i] != NULL && strstr(name_Stations, activeClients[i]->statie)) {
                            char notif[1024];
                            if(atoi(dValue)==1) { snprintf(notif, 1024, "Atentie! Trenul %s are intarziere de un minut.", trainNr); }
                            else if(atoi(dValue)>1) { snprintf(notif, 1024, "Atentie! Trenul %s are intarziere de %s minute",trainNr, dValue); }
                            else{ int nr=atoi(dValue)*(-1); snprintf(notif, 1024, "Atentie! Trenul %s vine mai devreme cu %d minute", trainNr, nr); }
                            if (write(activeClients[i]->cl, notif, 1024) <= 0) {
                                fprintf(stderr, "Error notifying client: %s\n", strerror(errno));
                            }
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    break;
                }
            }
        }
    }
    else if(strncmp(buf, "cautaTren", 9) == 0) {
        if(strcmp(td->username, "") == 0) strcpy(buf, "Nu sunteti logat. Folositi <login>");
        else if(strcmp(td->statie, "") == 0) strcpy(buf, "Nu ati introdus statia curenta.");
        else{
        sscanf(buf, "cautaTren %s", trainNr);
        snprintf(buf, 1024, "~ Trenul %s ~\n", trainNr); ok = 0;
        for(xmlNodePtr trains = root->children; trains!=NULL; trains = trains->next) {
            if(xmlStrEqual(trains->name, BAD_CAST trainNr)) {
                ok = 1;
                xmlNodePtr stations = xmlFirstElementChild(trains);
                strcat(buf, (char*)trains->name); strcat(buf, " ");
                xmlNodePtr st = xmlFirstElementChild(stations);
                while(st!=NULL) {
                    strcat(buf, " - "); strcat(buf, (char*)st->name); strcat(buf, "("); 
                    xmlChar *stationContent = xmlNodeGetContent(st);
                    strcat(buf, (char*)stationContent);
                    xmlFree(stationContent);
                    strcat(buf, ")"); st = xmlNextElementSibling(st);
                }
            }
        }
        if(ok == 0) strcpy(buf, "Trenul nu a fost gasit in baza de date.");}
    }
    else if(strncmp(buf, "cautaRuta", 9) == 0) {
        ok = 0;
        sscanf(buf, "cautaRuta %s %s", s1, s2);
        snprintf(buf, 1024, "~ Ruta %s - %s ~\n", s1, s2);
        for (xmlNodePtr trains = xmlFirstElementChild(root); trains != NULL; trains = xmlNextElementSibling(trains)) {
            int oktrain = 0;
            xmlNodePtr stations = xmlFirstElementChild(trains);
            xmlNodePtr st = xmlFirstElementChild(stations);
            while(st!=NULL) {
                if(strcmp(s1, (char*)st->name) == 0) break;
                st = xmlNextElementSibling(st);
            }
            while(st!=NULL) {
                if(strcmp(s2, (char*)st->name) == 0) {ok =1; oktrain = 1; break;}
                st = xmlNextElementSibling(st);
            }
            if(oktrain == 1) {
                strcat(buf, (char*)trains->name); strcat(buf, " ");
                xmlNodePtr station = xmlFirstElementChild(stations);
                while(strcmp(s1, (char*)station->name) != 0) station = xmlNextElementSibling(station);
                do {
                    strcat(buf, " - "); strcat(buf, (char*)station->name); strcat(buf, "("); 
                    xmlChar *stationContent = xmlNodeGetContent(station);
                    strcat(buf, (char*)stationContent);
                    xmlFree(stationContent);
                    strcat(buf, ")"); station = xmlNextElementSibling(station);
                }while(strcmp(s2,(char*)station->name) != 0);
                strcat(buf, " - "); strcat(buf, (char*)station->name); strcat(buf, "("); 
                xmlChar *stationContent = xmlNodeGetContent(station);
                strcat(buf, (char*)stationContent);
                xmlFree(stationContent);
                strcat(buf, ")"); strcat(buf, "\n");
            }
        }
        if(ok == 0) strcpy(buf, "Ruta nu a fost gasita in baza de date.");
    }
    else if(strncmp(buf, "login", 5) == 0) {
        sscanf(buf, "login %s %s", username, password);
        hash = hashpswrd(password);
        if (authentication(td->db, username, hash))
        {
            snprintf(update, sizeof(update), "UPDATE TUsers SET Status=1 WHERE Username='%s'", username);
            if (sqlite3_exec(td->db, update, NULL, NULL, NULL) != SQLITE_OK)
            {
                fprintf(stderr,"Error updating user status: %s\n", sqlite3_errmsg(td->db));
            }
            strcpy(td->username, username);
            strcpy(buf, "Logat.");
         }
         else
            strcpy(buf, "Eroare la logare.");
    }
    else if(strcmp(buf, "quit") == 0 || strcmp(buf, "logout") == 0) {
        snprintf(update, sizeof(update), "UPDATE TUsers SET Status=0 WHERE Username='%s'", td->username);
        if (sqlite3_exec(td->db, update, NULL, NULL, NULL) != SQLITE_OK)
        {
            fprintf(stderr, "Error updating user status: %s\n", sqlite3_errmsg(td->db));
        }
        strcpy(td->username, "");
        strcpy(td->statie, "");
        if(strcmp(buf, "quit") == 0)
            {strcpy(buf, "Quitting..."); removeClient(td);} 
        else 
            strcpy(buf, "Deconectat.");
    }
    else if(strncmp(buf, "sign-in", 7) == 0) {
        sscanf(buf, "sign-in %s %s", username, password);
        hash = hashpswrd(password);
        if(authentication(td->db, username, hash)) {
            strcpy(buf, "This user is already signed in.");        
        }
        else {
            strcpy(td->username, username);
            snprintf(insert, sizeof(insert), "INSERT INTO TUsers (username, password, Status) VALUES('%s', %d, 1)", username, hash);
            if (sqlite3_exec(td->db, insert, NULL, NULL, NULL) != SQLITE_OK)
            {
                fprintf(stderr,"Error updating user status: %s\n", sqlite3_errmsg(td->db));
            }
            strcpy(buf, "Inregistrat.");
        }
    }
    else if(strncmp(buf, "statie", 6) == 0) {
        sscanf(buf, "statie %s", td->statie);
        snprintf(buf, 1024, "Statia curenta: %s", td->statie);
    }
    else if(strcmp(buf, "statii") == 0) {
        buf[0]='\0';
        FILE *file = fopen("statii.txt", "r");
        if (file == NULL) {
            perror("[server]Eroare la deschiderea fisierului statii.txt\n");
            return;
        }
        char line[255];
        memset(line, 0, sizeof(line));
        memset(buf, 0, 1024);
        while (fgets(line, sizeof(line), file) != NULL) 
            strcat(buf, line);
        fclose(file);
    }
    else {
        buf[0]='\0';
        strcpy(buf, "Eroare: Comanda invalida. Pentru sintaxa folositi commands.");
        buf[strlen(buf)]='\0';    
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
}


int main () //++++++++++++++ M A I N +++++++++++++++++
{
    struct sockaddr_in server;
    struct sockaddr_in from;	
    int sd;
    pthread_t th[100]; 
    int i=0;
    sqlite3 *db;
    if (sqlite3_open("utilizatori.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    }
    pthread_t onTrId;
    pthread_create(&onTrId, NULL, checkTrains, NULL);
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
        perror ("[server]Eroare la socket().\n");
        return errno;
    }
    int on=1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
    /* pregatirea structurilor de date */
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));
  
    server.sin_family = AF_INET;	

    server.sin_addr.s_addr = htonl (INADDR_ANY);

    server.sin_port = htons (PORT);
  
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1) {
        perror ("[server]Eroare la bind().\n");
        return errno;
    }
    if (listen (sd, 2) == -1) {
        perror ("[server]Eroare la listen().\n");
        return errno;
    }
    while (1) {
      int client;
      thData *td; 
      int length = sizeof(from);
      printf ("[server]Asteptam la portul %d...\n",PORT);
      fflush (stdout);
      if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0) {
	      perror ("[server]Eroare la accept().\n");
	      continue;
      }
      td=(struct thData*)malloc(sizeof(struct thData));	
      td->idThread=i++;
      td->cl=client;
      td->db = db;
      strcpy(td->username, "");
      strcpy(td->statie, "");
      addClient(td);

      pthread_create(&th[i], NULL, &treat, td);	      			
    }
    sqlite3_close(db);
    return 0;
}				

static void *treat(void * arg) {		
	struct thData tdL; 
	tdL= *((struct thData*)arg);	
	printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
	fflush (stdout);		 
	pthread_detach(pthread_self());		
	raspunde((struct thData*)arg);
	close ((intptr_t)arg);
	return(NULL);	
};


void raspunde(thData *td) {
    sqlite3 *db = td->db;
    while (1) {
        char buf[1024];
        if (read(td->cl, buf, sizeof(buf)) <= 0) {
            printf("[Thread %d]\n", td->idThread);
            perror("Eroare la read() de la client.\n");
            break; 
        }

        buf[sizeof(buf) - 1] = '\0';

        printf("[Thread %d] Mesajul a fost receptionat: %s\n", td->idThread, buf);

        comanda(buf, td);
        if(strstr(buf, "setDelay") == NULL || strcmp(buf, "Trenul nu este in mers.") == 0) {
            if (write(td->cl, buf, sizeof(buf)) <= 0) {
                printf("[Thread %d] ", td->idThread);
                perror("[Thread] Eroare la write() catre client.\n");
                break;
            } else {
                //printf("[Thread %d] Mesajul a fost trasmis cu succes.\n", td->idThread);
            }
            if (strcmp(buf, "Quitting...") == 0) {
                printf("[Thread %d] Clientul a cerut închiderea conexiunii.\n", td->idThread);
                break;
            }
        }
    }
    close(td->cl);
}
unsigned int hashpswrd(const char *str) {
    unsigned int hash = 0;

    while (*str) {
        hash = (hash * 31) + *str++;
    }

    return hash;
}
int authentication(sqlite3 *db, const char *username, int hash){
    sqlite3_stmt *stmt;
    char select[101];
    snprintf(select, sizeof(select), "SELECT * FROM TUsers WHERE Username='%s' AND Password=%d AND Status=0", username, hash);

    if(sqlite3_prepare_v2(db, select, -1, &stmt, NULL) != SQLITE_OK){
        fprintf(stderr, "Error preparing SQL stmt: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    int result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = 1;
    sqlite3_finalize(stmt);
    return result;
}
int ctoi(char time[6]) {
    char min[5];
    int minus = 0, nr_min;
    if(time[0] == '-') { sscanf(time, "-%s", min); minus = 1; }
    nr_min= atoi(min); 
    if(minus == 1) nr_min = nr_min* (-1);
    return nr_min;
}
int h_min(char time[6]) {
    char hour[3], min[3];
    int nr_h, nr_min;
    sscanf(time, "%2s:%2s", hour, min);
    nr_h=atoi(hour);
    nr_min=atoi(min);
    return nr_h * 60 + nr_min;
}
void min_h(int min, char time[6]) {
    snprintf(time, 6, "%d:%02d", min/60, min%60);
}
static void* checkTrains(void *arg) {
    int begin, end, d, minutes, ok;
    char update[3], dValue[3];
    xmlDocPtr doc;
    xmlNodePtr root, train, stations, st, delay, status;
    xmlChar *content;

    while (1) {
        doc = xmlReadFile("trNr.xml", NULL, 0);
        if (doc == NULL) {
            fprintf(stderr, "Failed to parse XML file.\n");
            return NULL;
        }
        root = xmlDocGetRootElement(doc);
        if (root == NULL) {
            fprintf(stderr, "Empty document.\n");
            xmlFreeDoc(doc);
            xmlCleanupParser();
            return NULL;
        }

        time_t current_time = time(NULL);
        struct tm *time_info = localtime(&current_time);
        int hour = time_info->tm_hour;
        int min = time_info->tm_min;
        minutes = 60 * hour + min;

        for (train = xmlFirstElementChild(root); train != NULL; train = xmlNextElementSibling(train)) {
            ok = 0;
            stations = xmlFirstElementChild(train);
            st = xmlFirstElementChild(stations);
            content = xmlNodeGetContent(st);
            if (content == NULL) {
                fprintf(stderr, "No content found.\n");
                continue;
            }
            begin = h_min((char *)content);
            xmlFree(content);
            while (xmlNextElementSibling(st) != NULL)
                st = xmlNextElementSibling(st);
            content = xmlNodeGetContent(st);
            if (content == NULL) {
                fprintf(stderr, "No content found.\n");
                continue;
            }
            end = h_min((char *)content);
            xmlFree(content);
            delay = xmlNextElementSibling(stations);
            content = xmlNodeGetContent(delay);
            if (content == NULL) {
                fprintf(stderr, "No content found.\n");
                continue;
            }
            d = ctoi((char *)content);
            xmlFree(content);
            if (minutes >= begin && minutes < end + d)
                strcpy(update, "1");
            else {
                strcpy(update, "0");
                strcpy(dValue, "0");
                xmlNodeSetContent(delay, BAD_CAST dValue);
            }
            status = xmlNextElementSibling(delay);
            if (status != NULL) {
                xmlNodeSetContent(status, BAD_CAST update);
                xmlSaveFormatFile("trNr.xml", doc, 1);
            }
        }
        xmlFreeDoc(doc);
        xmlCleanupParser();
        sleep(60);
    }
    return(NULL);
};
