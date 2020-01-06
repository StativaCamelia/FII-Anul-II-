#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "sqlite3.h"
#include <libxml/parser.h>


#define PORT 2019 //PORTUL FOLOSIT
#define MSGSIZE 500
#define MAXCLIENTS 100
#define INCIDENTS_POINTS 7
#define NORMAL_TRAFIC 30
#define INCIDENT_SPEED_LIMIT 20
#define INFINIT 10e+8
#define NODES 11
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


//baza de date
sqlite3* db;

//structura folosita de clienti
static int uid = 10;
//client info0.
volatile int logat = 0;
volatile int route_change = 0;
volatile int cancel = 0;
volatile int news_option = 0;
char viteza[50] = {0};
volatile int viteza_int = 0;
char locatie_start[50] = {0};
char locatie_curenta[50] = {0};
char locatie_stop[50] = {0};
int Graph[NODES][NODES];
int path[NODES];
int start_route = -1;

//prototip functie listener:

void *recv_msg(void *arg);

//prototip send
void send_function(char *msg_de_trimis);

//prototip functii incidente:
int add_event(const char *primit);
void *send_incidents_function(void *arg);

//prototip functii news:
void get_news(char *mesaj);
void *send_news(void *arg);

//functii comenzi:
void functie_help_main(char* raspuns);
void functie_help_login(char* raspuns);
int functie_login(const char* primit,char *raspuns, int logat, char* username);
int functie_sign_in(const char* primit, char* raspuns, int logat, char* username);
void functie_update_settings(const char* primit,char* raspuns, char* username);
int functie_delogare(char *raspuns, char* username);

//speed
int check_speed_limit(char viteza[]);
void speed_advertisment(const char *primit,char* raspuns);
void send_speed_limit(char *raspuns);
void update_speedlimit_incident(const char* primit);
void update_speedlimit_aglomeration(int nodeID);
int check_cars_no(int nodeID);
void set_normal_speed();

void add_some_cars();

//pozitie
void initializare_pozitie(const char *primit, char* raspuns);
void *parcurgere_traseu(void *arg);

//aditional
int pregatire_raspuns(const char *primit, char *raspuns, int logat, char* username);
void get_lines(const char *primit, char* line1, char* line2);
void get_line(const char *primit, char* line1);
void sigchld_handler(int s);
char *removeSpaces(const char *string);
static int callback(void *data, int argc, char **argv, char **azColName);
void hash_password(char *pass);
char *get_streets(const char *primit);
char *street_name(char* data, const char* primit);
void update_cars_no(int nodeID, int increase);

int get_nodeID(char *streetName);
int get_path_cost(int from_NodeID, int to_NodeID);
int get_distance(int from_nodeID, int to_nodeId);
int get_incidents_points(int to_nodeID);
int get_number_of_cars_points(int to_nodeID);
int get_minim_cars();
int check_no_cars();

void get_name(int nodeID, char* next_street);
int dijkstra(int Graph[][NODES], int location, int target, int path[NODES]);
void get_name_from_nodeID(int nodeID, char* curretn_location);
void initializare_matrice_costuri(int Grapth[][NODES]);
extern int errno;


void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


static int callback(void *data, int argc, char **argv, char **azColName)
{
   int i;
   char *rasp = (char*)data;
   for(i = 0; i<argc; i++)
   {
      strcat(rasp, azColName[i]);
      strcat(rasp, ":");
      argv[i] ? strcat(rasp,argv[i]) : strcat(rasp,"NULL");
      strcat(rasp, "\n");
   }
   strcat(rasp, "\n");
   return 0;
}


static int callback_without_name(void *data, int argc, char **argv, char **azColName)
{
   int i;
   char *rasp = (char*)data;
   for(i = 0; i<argc; i++)
   {
      argv[i] ? strcat(rasp,argv[i]) : strcat(rasp,"NULL");
      strcat(rasp, "\n");
   }
   return 0;
}


//Ce comenzi poate sa primeasca serverul:
char login[] = "LOG";
char sign_in[] = "SIG";
char traffic_info[] = "TRF";
char update_settings[] = "SET";
char locatie[] = "LOC";
char update_speed[] = "SPD";
char delog[] = "DLG";
char quit[] = "QUI";
char help[] = "HEL";
char confirm_dest[] = "DSE";
char destinatie[] = "DST";

int main(int argc, char*argv[])
{
    int data_base = sqlite3_open("Monitorizare_Trafic.db", &db);
    if(data_base)
    {
        fprintf(stderr, "Baza de date nu poate fi deschisa: %s\n", sqlite3_errmsg (db));
        return 0;
    }

    //structura folosita de server
    struct sockaddr_in server;

    //structura pentru clienti
    struct sockaddr_in from;

    //descriptor socket server
    int sock_serv;

    //descriptor socket client
    int sock_client;

    //creare socket
    if((sock_serv = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Eroare la creare socket\n");
        return errno;
    }

    int on=1;
    setsockopt(sock_serv,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

    //pregatire structuri date
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    //pregatim structura server1
    //familie socket
    server.sin_family = AF_INET;
    //acceptam orice adresa
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    //setam portul
    server.sin_port = htons(PORT);

    //atasam socket
    if (bind (sock_serv, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

    //putem serverul sa asculte
    if(listen(sock_serv,10) == -1)
    {
        perror("[server] Eroare la listen\n");
        return errno;
    }
    //signal
    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("[server] Astept clientii la portul: %d\n",PORT);
    //incepem sa servim in mod concurent clientii

    while(1)
    {
        fflush(stdout);

        //acceptam un client
        int lenght = sizeof(from);
        sock_client = accept(sock_serv, (struct sockaddr *)&from, &lenght);
        if(sock_client < 0)
        {
            perror("[server] Eroare la accept\n");
            continue;
        }
        dup2(sock_client,uid);
        uid++;
        int pid = fork();

        if(pid == 0)
        {
                fflush(stdout);
                fflush(stdin);
                memset(viteza, 0, sizeof(viteza));
                close(sock_serv);

                int sock = uid-1;
                pthread_t recv_thread, send_thread, send_incidents, update_location;

                if(pthread_mutex_init(&lock, NULL))
                {
                    printf("Initializarea mutex nu a reusit");
                }

                pthread_create(&recv_thread, NULL, &recv_msg, (void*)sock);
                pthread_create(&send_thread, NULL, &send_news, (void*)sock);
                pthread_create(&send_incidents, NULL, &send_incidents_function, (void*)sock);
                pthread_create(&update_location, NULL, &parcurgere_traseu, (void*)sock);

                pthread_join(update_location, NULL);
                pthread_join(send_incidents, NULL);
                pthread_join(send_thread, NULL);
                pthread_join(recv_thread, NULL);
                
                pthread_mutex_destroy(&lock);

                if(cancel == 1)
                    exit(1);

        }//end if fork

        set_normal_speed();
        add_some_cars();

        close(sock_client);
        close(uid-1);
    }
    sqlite3_close(db);
}


void get_name_from_nodeID(int nodeID, char *current_location)
{
    memset(current_location, 0, sizeof(current_location));
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    sprintf(sql, "SELECT StreetName FROM Streets WHERE NodeID = %d;", nodeID);
    sql[strlen(sql)] = '\0';

    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    data[strlen(data)-1] = '\0';
    strcpy(current_location, removeSpaces(data));
}

int check_close_streets(int status)
{
    char msg_de_trimis[MSGSIZE];
    if(status < start_route-1)
    {
        if(check_no_cars(status+1) > NORMAL_TRAFIC)
        {
           return 1;
        }
        else
        {
            return 0;
        }
        
    }
}


void *parcurgere_traseu(void *sock)
{
    int sock_d = (int)sock;
    char msg_de_trimis[MSGSIZE];
    int status_parcurs_total = 1;
    float status_parcurs_per_strada = 0;
    int distance;
    char next_street[MSGSIZE];

    while(cancel == 0)
    {  
        if(start_route >= 0 && logat == 1)
        {    
            printf("%s, %s\n", locatie_start, locatie_stop);
            if(route_change == 1)
            {
                status_parcurs_total = 1;
                status_parcurs_per_strada = 0;
                route_change = 0;
                continue;
            }
            if(check_close_streets(status_parcurs_total+1)==1)
            {
                    memset(msg_de_trimis, 0, sizeof(msg_de_trimis));
                    memset(next_street, 0, sizeof(next_street));
                    get_name(status_parcurs_total+1, next_street);
                    sprintf(msg_de_trimis, "TRF: Se circula greu datorita aglomeratiei pe strada %s", next_street);
                    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
                    send_function(msg_de_trimis);
            }
            if(start_route == 0)
            {
                memset(msg_de_trimis, 0, sizeof(msg_de_trimis));
                sprintf(msg_de_trimis, "LOF: %s", locatie_curenta);
                msg_de_trimis[strlen(msg_de_trimis)] = '\0';
                send_function(msg_de_trimis);
                start_route = -1;
            }
            else
            {   distance = get_distance(path[status_parcurs_total],path[status_parcurs_total+1]);
                if(status_parcurs_per_strada >= distance)
                {
                    memset(msg_de_trimis, 0, sizeof(msg_de_trimis));
                    status_parcurs_per_strada = 0;
                    status_parcurs_total++;
                    start_route--;

                    get_name_from_nodeID(path[status_parcurs_total], locatie_curenta);
                    update_cars_no(path[status_parcurs_total], 1);
                    update_cars_no(path[status_parcurs_total-1], 0);
                    if(check_close_streets(status_parcurs_total+1)==1)
                    {
                        memset(msg_de_trimis, 0, sizeof(msg_de_trimis));
                        memset(next_street, 0, sizeof(next_street));
                        get_name(status_parcurs_total+1, next_street);
                        sprintf(msg_de_trimis, "TRF: Se circula greu datorita aglomeratiei pe strada %s", next_street);
                        msg_de_trimis[strlen(msg_de_trimis)] = '\0';
                        send_function(msg_de_trimis);
                    }
         
                    send_speed_limit(msg_de_trimis);
                    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
                    send_function(msg_de_trimis);
                }
                else
                {
                    status_parcurs_per_strada = status_parcurs_per_strada + (double)viteza_int*0.6;
                    sleep(60);
                }
            }

        }
    }
}


void get_incidents(char* incidents, int last_check)
{
    char sql[MSGSIZE];
    char data[100];
    char *zErrMsg = 0;
    int rc;

    memset(data, 0, sizeof(data));
    memset(sql, 0, sizeof(sql));
    sql[strlen(sql)] = '\0';

    sprintf(sql, "SELECT Incident FROM Incidente WHERE Timestamp > %d;\0", last_check);

    rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);

    fflush(stdout);

    data[strlen(data)] = '\0';

    if( rc != SQLITE_OK)
    {
        printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else
    {
        strcat(incidents, "TRF:");
        strcat(incidents, removeSpaces(data));
    }
    memset(data, 0, sizeof(data));
    memset(sql, 0, sizeof(sql));
}


void *send_incidents_function(void *sock)
{
    int sock_d = (int)sock;
    char incidents[MSGSIZE];
    int current_time = time(NULL);
    int last_check = time(NULL);

    while(cancel == 0)
    {
        memset(incidents, 0, sizeof(incidents));
        fflush(stdin);
        fflush(stdout);

        if(logat == 1)
        {
            get_incidents(incidents, last_check);
            if(strlen(incidents) > 4)
            {
                incidents[strlen(incidents)] = '\0';
                send_function(incidents);
            
            }
            last_check = current_time+3 ;
        }
        current_time = time(NULL);
        sleep(7);
    }
}


void generate_random(char *compare)
{
    srand(time(NULL));
    int rand_num;
    rand_num = rand()%4 + 1;
    if(rand_num == 1)
        strcpy(compare,"n1");
    else if(rand_num == 2)
        strcpy(compare, "n2");
    else if(rand_num == 3)
        strcpy(compare, "n3");
    else
        strcpy(compare, "n4");
}


void get_news(char *news)
{
    char compare[MSGSIZE];
    memset(compare, 0, sizeof(compare));

    fflush(stdin);
    fflush(stdout);
    strcat(news,"NEW:");

    generate_random(compare);

    xmlDoc *document;
    xmlNode *root, *node, *first_child, *first_inner_child, *inner_node;

    char *filename = "news.xml";
    document = xmlReadFile(filename, NULL, 0);
    root = xmlDocGetRootElement(document);
    first_child = root->children;
    for (node = first_child; node; node = node->next)
        {
            if(strstr(node->name,compare) != NULL)
            {
                first_inner_child = node ->children;
                for(inner_node = first_inner_child; inner_node; inner_node = inner_node -> next)
                    {
                        if(strstr(inner_node->name,"text"))
                            continue;
                        strcat(news, inner_node->name);
                        strcat(news, ":");
                        strcat(news, xmlNodeGetContent(inner_node));
                        strcat(news, "\n");
                    }
                break;
            }
        }
}


void *send_news(void* sock)
{
    int sock_d = (int)sock;
    char news[MSGSIZE];

    while(cancel == 0)
    {
        memset(news, 0, sizeof(news));
        fflush(stdin);
        fflush(stdout);

        if(news_option == 1)
        {
            get_news(news);
            news[strlen(news)] = '\0';
            send_function(news);
            sleep(3600);
        }
    }
}


void send_function(char *msg_de_trimis)
{

    int lungime_int;
    char lungime_str[3];
    pthread_mutex_lock(&lock);

    lungime_int = strlen(msg_de_trimis);
    sprintf(lungime_str,"%d", lungime_int-1);

    fflush(stdin);
    fflush(stdout);

    if(write(uid-1, lungime_str, sizeof(lungime_str)) <= 0)
    {
        perror("[server] Mesajul cu lungimea NU a fost trimis");
        return errno;
    }

    if(write(uid-1, msg_de_trimis, lungime_int) <= 0)
    {
        perror("[server] Mesajul cu date NU a fost trimis");
        return errno;
    }

    fflush(stdin);
    fflush(stdout);

    pthread_mutex_unlock(&lock);
}


void *recv_msg(void *sock)
{
        int sock_d = (int)sock;
        char lungime_str[3];
        int lungime_int;
        int stop;

        while(cancel == 0)
        {

            fflush(stdout);

            memset(lungime_str, 0, sizeof(lungime_str));

            if((stop=read(sock_d, lungime_str, sizeof(lungime_str)))<0)
            {
                perror("[server] Eroare la citirea lungimii in server\n");
                return errno;
            }

            if(stop == 0)
            {
                cancel = 1;
                break;
            }

            lungime_int = atoi(lungime_str);

            char *msg_primit = malloc(lungime_int+1);
            memset(msg_primit, 0, lungime_int+1);
            int size = lungime_int;

            while(size > 0)
            {
                if((stop=read(sock_d, msg_primit, lungime_int))<0)
                {
                    perror("[server] Eroare la citirea mesajului in server");
                    return errno;
                }
                if(stop == 0)
                {
                    cancel = 1;
                    break;
                }
                else
                {
                    size = size - stop;
                }
            }
            printf("%s\n", msg_primit);
            if(stop == 0)
            {
                cancel = 1;
                break;
            }

            //trimitere mesaje:
            char msg_de_trimis[MSGSIZE];
            char username[100];
            memset(msg_de_trimis, 0, sizeof(msg_de_trimis));

            if(strstr(msg_primit, quit) != NULL)
            {
                cancel = 1;
                sprintf(msg_de_trimis, "QUI: Urmeaza sa va deconectati...");
                msg_de_trimis[strlen(msg_de_trimis)] = '\0';
                send_function(msg_de_trimis);
            }
            else
            {
                logat = pregatire_raspuns(msg_primit, msg_de_trimis, logat, username);
            }
            free(msg_primit);
        }
}


void get_line(const char *primit, char* line1)
{
    memset(line1, 0, sizeof(line1));
    char copy[MSGSIZE] = {0};
    strcpy(copy, primit);
    char *ptr = strtok(copy, "\n");
    ptr = strtok(NULL, "\n");
    sprintf(line1, "%s", ptr);
    removeSpaces(line1);
}


int check_street_exists(char *possible_dest)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    sprintf(sql, "SELECT * FROM Streets WHERE StreetName = '%s';", possible_dest);
    sql[strlen(sql)] = '\0';

    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    if(strlen(data)> 0 )
        return 1;
    else
    {
        return 0;
    }
    

}


void set_destination(const char* primit, char* msg_de_trimis)
{
    char possible_dest[MSGSIZE];
    get_line(primit, possible_dest);
    if(check_street_exists(possible_dest))
    {
        memset(locatie_stop, 0, sizeof(locatie_stop));
        strcpy(locatie_stop, possible_dest);
        start_route = dijkstra(Graph,get_nodeID(locatie_start), get_nodeID(locatie_stop), path);
        route_change = 1;
        memset(msg_de_trimis, 0, sizeof(0));
        sprintf(msg_de_trimis,"DSE:%s ", locatie_stop);
        msg_de_trimis[strlen(msg_de_trimis)] = '\0';
        send_function(msg_de_trimis);
    }  
    else 
    {
        memset(msg_de_trimis, 0, sizeof(0));
        sprintf(msg_de_trimis,"ERR:Strada %s nu exista", possible_dest);
        msg_de_trimis[strlen(msg_de_trimis)] = '\0';
        send_function(msg_de_trimis);
    }  
}


int pregatire_raspuns(const char *primit, char *msg_de_trimis, int logat, char* username)
{
    fflush(stdout);

    if(strstr(primit, login) != NULL)
    {
        logat = functie_login(primit, msg_de_trimis, logat, username);
    }
    else if(strstr(primit, sign_in) != NULL)
    {
        logat = functie_sign_in(primit, msg_de_trimis, logat, username);
    }
    else if(strstr(primit, help) != NULL && logat == 0)
    {
        logat = logat;
        functie_help_login(msg_de_trimis);
    }
    else if(strstr(primit, help) != NULL && logat == 1)
    {
        logat = logat;
        functie_help_main(msg_de_trimis);
    }
    else if(strstr(primit, update_settings) != NULL && logat == 1)
    {
        logat = logat;
        functie_update_settings(primit, msg_de_trimis, username);
    }
    else if(strstr(primit, traffic_info) != NULL && logat == 1)
    {
        logat = logat;
        if(add_event(primit) == 1)
            update_speedlimit_incident(primit);     
    }
    else if(strstr(primit, update_speed)!= NULL && logat == 1)
    {
        logat = logat;
        speed_advertisment(primit, msg_de_trimis);
    }
    else if(strstr(primit, locatie) != NULL && logat == 1)
    {
        logat = logat;
        initializare_pozitie(primit, msg_de_trimis);
        start_route = dijkstra(Graph,get_nodeID(locatie_start), get_nodeID(locatie_stop), path);
    }
    else if(strstr(primit, delog) != NULL && logat == 1)
    {
        logat = functie_delogare(msg_de_trimis, username);
    }
    else if(strstr(primit, destinatie) != NULL && logat == 1)
    {
        logat = logat;
        set_destination(primit, msg_de_trimis);
    }
    else
    {
        logat = logat;
        sprintf(msg_de_trimis, "ERR: Comanda introdusa nu exista");
        msg_de_trimis[strlen(msg_de_trimis)] = '\0';
        send_function(msg_de_trimis);
    }

    return logat;
}


void initializare_pozitie(const char *primit, char* msg_de_trimis)
{

    memset(locatie_start,0, sizeof(locatie_start));
    memset(locatie_stop, 0, sizeof(locatie_stop));

    get_lines(primit, locatie_start, locatie_stop);

    memset(locatie_curenta, 0, sizeof(locatie_curenta));
    strcpy(locatie_curenta, locatie_start);
    update_cars_no(get_nodeID(locatie_start), 1);
    send_speed_limit(msg_de_trimis);
    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
    send_function(msg_de_trimis);
}


char *removeSpaces(const char *string) {

    while(isspace((unsigned char)string[0]))
        string++;
    char *final = strdup(string);
    int length = strlen(final);
    while(length > 0 && isspace((unsigned char)final[length-1]))
        length--;
    final[length+1] = '\0';
    return final;
}


void send_speed_limit(char* msg_de_trimis)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    int res;
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    fflush(stdout);

    memset(data, 0 ,sizeof(data));
    memset(sql, 0, sizeof(data));
    memset(msg_de_trimis, 0, sizeof(msg_de_trimis));

    locatie_curenta[strlen(locatie_curenta)] = '\0';

    sprintf(sql, "SELECT SpeedLimit from Streets WHERE StreetName = '%s';",locatie_curenta);

    fflush(stdout);

    sql[strlen(sql)] = '\0';

    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);

    fflush(stdout);

    data[strlen(data)] = '\0';

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    else
    {
        sprintf(msg_de_trimis,"LOC:Va aflati pe strada: %s, viteza maxima admisa este: %s", locatie_curenta, removeSpaces(data));
        msg_de_trimis[strlen(msg_de_trimis)] = '\0';

    }
    memset(data, 0, sizeof(data));
    memset(sql, 0, sizeof(sql));
}


int check_speed_limit(char *viteza)
{

    char sql[MSGSIZE];
    char data[MSGSIZE];
    int res;
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    fflush(stdout);

    memset(data, 0 ,sizeof(data));
    memset(sql, 0, sizeof(data));

    sprintf(sql, "SELECT SpeedLimit from Streets WHERE StreetName = '%s' and SpeedLimit < %s;",locatie_curenta, viteza);

    rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);

    data[strlen(data)] = '\0';
    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    else if(strlen(data) > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


void speed_advertisment(const char *primit, char* msg_de_trimis)
{

    int viteza_ok = 0;

    memset(viteza, 0, sizeof(viteza));
    memset(msg_de_trimis, 0, sizeof(msg_de_trimis));
    fflush(stdout);

    sprintf(viteza, "%s", primit+strlen(update_speed))+1;
    viteza_int = atoi(viteza);

    viteza_ok = check_speed_limit(viteza);
    if(viteza_ok == 1)
    {
        sprintf(msg_de_trimis, "SPD:Ati depasit viteza de limita impusa pe sectorul de drum: %s", locatie_curenta);
        msg_de_trimis[strlen(msg_de_trimis)] = '\0';
        send_function(msg_de_trimis);
    }
}


int add_event(const char *primit)
{
    char street[100];
    char type[100];
    get_lines(primit, type, street);

    if(check_street_exists(street)==0)
    {
        char msg_de_trimis[MSGSIZE];
        sprintf(msg_de_trimis, "ERR: Strada introdusa nu exista");
        send_function(msg_de_trimis);
        return 0;
    }
    else if(strstr(type, "Radar") == NULL && strstr(type, "Accident") == NULL && strstr(type, "Aglomeratie") == NULL && strstr(type, "Trafic Normal") == NULL)
    {
        char msg_de_trimis[MSGSIZE];
        sprintf(msg_de_trimis, "ERR: Tipul de eveniment introdus nu exista");
        send_function(msg_de_trimis);
        return 0;
    }
    else
    {
        printf("HERE");
  
        char sql[MSGSIZE];
        char
        data[MSGSIZE];
        data[0] = 0;
        sql[0] = 0;
        char *zErrMsg = 0;
        int rc;

        fflush(stdout);
        char carbon_copy[MSGSIZE];
        strcpy(carbon_copy, primit);
        memset(data, 0, sizeof(data));
        memset(sql, 0, sizeof(sql));
        int timestamp = time(NULL);

        sprintf(sql, "INSERT INTO Incidente (Incident, Timestamp) VALUES ('%s', %d);", carbon_copy+strlen(traffic_info), timestamp);
        sql[strlen(sql)] = '\0';

        rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);

        fflush(stdout);

        data[strlen(data)] = '\0';
        if( rc != SQLITE_OK)
        {
                printf("SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }
        memset(data, 0, sizeof(data));
        memset(sql, 0, sizeof(sql));
    }
    return 1;
}


void functie_update_settings(const char *primit, char* msg_de_trimis, char* username)
{
    char *option = primit + strlen(update_settings);
    int option_int;
    char data[MSGSIZE];
    char sql[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    if(strstr(option, "Yes") != NULL)
    {
        option_int = 1;
    }
    else if(strstr(option, "No") != NULL)
    {
        option_int = 0;
    }


    memset(sql, 0, sizeof(sql));
    memset(msg_de_trimis, 0,sizeof(msg_de_trimis));
    memset(data, 0, sizeof(data));
    fflush(stdout);

    sprintf(sql,"UPDATE Clienti SET Options = %d Where Username = '%s';", option_int, username);
    sql[strlen(sql)] = '\0';

    rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);

    fflush(stdout);

    data[strlen(data)] = '\0';

    if( rc != SQLITE_OK)
    {
        printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else
    {
        sprintf(msg_de_trimis, "UOK: Update-ul Setarilor s-a facut cu succes");
    }
    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
    send_function(msg_de_trimis);
    if(option_int == 1);
        news_option = 1;

    memset(data, 0, sizeof(data));
    memset(sql, 0, sizeof(sql));
}


void functie_help_login(char* msg_de_trimis)
{
    strcat(msg_de_trimis, "HLP:");
    strcat(msg_de_trimis, "Pentru Login introduce comanda <Login>, ulterior vi se vor solicita username-ul si parola, asociate contului\n");
    strcat(msg_de_trimis, "Pentru sign_in ntorduceti comanda <Sign in>, ulteror vi se vor solicita username-ul si parola noului cont\n");
    strcat(msg_de_trimis, "Pentru a parasi aplicatie introduceti comanda <Quit>");
    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
    send_function(msg_de_trimis);
}


void functie_help_main(char* msg_de_trimis)
{
    strcat(msg_de_trimis, "HLP:");
    strcat(msg_de_trimis, "Aplicatie este destinata monitorizarii traficului, drept urmare pozitia respectiv viteza dumneaavoastra sunt utilizate in acest scop\n");
    strcat(msg_de_trimis, "In acest fel urmeaza a fi instintati cu privire la evenimentele petrecute in trafic si respectiv cu privire la restrictiile de viteza\n");
    strcat(msg_de_trimis, "Daca doriti raportarea unui eveniment introduceti comanda <Trafic Info>, urmata de mesajul dumneavoastra");
    strcat(msg_de_trimis, "Daca doriti sa va abonati la stirile legate de vreme, statii peco, etc, introduceti comanda <Settings Update>");
    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
    send_function(msg_de_trimis);
}


void get_lines(const char *primit, char *line1, char *line2)
{

    memset(line1, 0, sizeof(line1));
    memset(line2, 0, sizeof(line2));
    char copy[MSGSIZE] = {0};
    strcpy(copy, primit);
    char *ptr = strtok(copy, "\n");
    ptr = strtok(NULL, "\n");
    sprintf(line1, "%s", ptr);
    removeSpaces(line1);
    line1[strlen(line1)] = '\0';
    ptr = strtok(NULL, "\n");
    sprintf(line2,"%s", ptr);
    line2[strlen(line2)] = '\0';
}


int functie_sign_in(const char *primit, char *msg_de_trimis, int logat, char *username)
{
    char pass[100];
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;

    if(logat == 1)
    {
        sprintf(msg_de_trimis, "INU:Utilizatorul %s este deja logat\n", username);
        logat = logat;
    }
    else
    {
        get_lines(primit, username, pass);
        //trebuie verificat daca exista sau nu
        sprintf(sql, "SELECT Username FROM Clienti;");
        sql[strlen(sql)] = '\0';

        memset(data, 0, sizeof(data));
        int rc;
        rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);

        fflush(stdout);

        data[strlen(data)] = '\0';
        if( rc != SQLITE_OK)
        {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        else if(strstr(data, username)!=NULL)
        {
            sprintf(msg_de_trimis,"INU:Username-ul %s nu este disponibil\n", username);
            logat = 0;
        }
        else
        {
            memset(sql, 0, sizeof(sql));
            memset(data, 0, sizeof(data));

            sql[0] = 0;
            hash_password(pass);

            sprintf(sql, "INSERT INTO Clienti (Username, Password, Options) VALUES('%s','%s',0);",username, pass);
            sql[strlen(sql)] = '\0';
            data[0] = 0;
            rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);

            fflush(stdout);
            if( rc != SQLITE_OK)
            {
                printf("SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }
            else
            {
                sprintf(msg_de_trimis, "IOK:Inregistrarea s-a facut cu succes\n");
                logat = 1;
            }

        }
        memset(data, 0, sizeof(data));
        memset(sql, 0, sizeof(sql));
    }

    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
    send_function(msg_de_trimis);
    return logat;
}


int functie_login(const char *primit, char *msg_de_trimis, int logat, char*username)
{
    char pass[100];
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;

    if(logat == 1)
    {
        sprintf(msg_de_trimis, "LNU:Utilizatorul %s este deja logat\n", username);
        logat = logat;
    }
    else
    {
        get_lines(primit, username, pass);
        hash_password(pass);
        sprintf(sql, "SELECT * from Clienti where Username = '%s' and Password = '%s';",username, pass);
        sql[strlen(sql)] = '\0';
        int rc;

        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);
        fflush(stdout);

        if( rc != SQLITE_OK)
        {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        else if (strstr (data, username))
        {
            sprintf (msg_de_trimis,"LOK:Utilizatorul %s s-a logat cu succes.\n", username);
            logat = 1;
        }
        else
        {
            sprintf(msg_de_trimis, "LNU:Username-ul sau parola nu corespund unui cont existent\n");
            logat = 0;
        }
        if(strstr(data, "Options:1") != NULL)
        {
            news_option = 1;
        }
    }
    memset(data, 0, sizeof(data));
    memset(sql, 0, sizeof(sql));
    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
    send_function(msg_de_trimis);
    return logat;

}

void hash_password(char *pass)
{
    char ch;
    int key = 3;
    for(int i=0; pass[i] != '\0'; ++i)
    {
        ch = pass[i];

        if(ch >= 'a' && ch <= 'z')
        {
            ch = ch + key;
            if(ch > 'z')
                ch = ch - 'z' + 'a' - 1;
        pass[i] = ch;
        }
        else if(ch >= 'A' && ch <= 'Z')
        {
            ch = ch + key;
            if(ch > 'Z')
                ch = ch - 'Z' + 'A' -1;
        pass[i] = ch;
        }
        else
        {
            pass[i] = pass[i];
        }

    }
}

int functie_delogare(char *msg_de_trimis, char *username)
{
    logat = 0;
    sprintf(msg_de_trimis, "DLG:Utilizatorul %s s-a deconectat", username);
    msg_de_trimis[strlen(msg_de_trimis)] = '\0';
    send_function(msg_de_trimis);
    return logat;
}

void update_speedlimit_incident(const char *primit)
{
    char street[100];
    char type[100];
    get_lines(primit, type, street);
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
  
    if(strstr(type,"Accident") != NULL || strstr(type,"Aglomeratie") != NULL)
    {
        sprintf(sql, "UPDATE Streets SET SpeedLimit = %d, Incident = 1 WHERE StreetName = '%s';",INCIDENT_SPEED_LIMIT,street);
        sql[strlen(sql)] = '\0';
        int rc;
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);
        if( rc != SQLITE_OK)
        {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    else  if(strstr(type, "Radar") != NULL)
    {
        sprintf(sql, "UPDATE Streets SET SpeedLimit = SpeedLimit_Normal, Incident = 1 WHERE StreetName = '%s';",INCIDENT_SPEED_LIMIT,street);
        sql[strlen(sql)] = '\0';
        int rc;
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);
        if( rc != SQLITE_OK)
        {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    else if(strstr(type, "Trafic normal") != NULL)
    {
        sprintf(sql, "UPDATE Streets SET SpeedLimit = SpeedLimit_Normal, Incident = 0 WHERE StreetName = '%s';",street);
        sql[strlen(sql)] = '\0';
        int rc;
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);
        if( rc != SQLITE_OK)
        {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }        
    }
    }


void set_normal_speed()
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;

    sprintf(sql, "UPDATE Streets SET SpeedLimit = SpeedLimit_Normal WHERE Incident = 0;");
    sql[strlen(sql)] = '\0';
    int rc;
    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }

}


void update_cars_no(int nodeID, int increase)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    if(increase == 1)
    {
        sprintf(sql, "UPDATE Paths SET Cars = Cars+1 WHERE From_node = %d;", nodeID);
        sql[strlen(sql)] = '\0';
        int rc;
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);

        if( rc != SQLITE_OK)
        {
                printf("SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }
    }
    else
    {
        sprintf(sql, "UPDATE Paths SET Cars = Cars-1 WHERE From_node = %d;", nodeID);
        sql[strlen(sql)] = '\0';
        int rc;
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);

        if( rc != SQLITE_OK)
        {
                printf("SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }
    }
    update_speedlimit_aglomeration(nodeID);
}


int check_no_cars(int nodeID)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    sprintf(sql, "SELECT Cars from Paths where From_node = %d;", nodeID);
    sql[strlen(sql)] = '\0';
    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
        printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    return atoi(data);
}

void get_name(int nodeID, char* next_street)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    sprintf(sql, "SELECT StreetName from Streets where NodeId = %d",nodeID);
    sql[strlen(sql)] = '\0';
    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    if( rc != SQLITE_OK)
    {
        printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    strcpy(next_street, data);
}


void update_speedlimit_aglomeration(int nodeID)
{

    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;
    if(check_no_cars(nodeID) > NORMAL_TRAFIC)
    {
        sprintf(sql, "UPDATE Streets SET SpeedLimit = %d WHERE NodeID = '%d';",INCIDENT_SPEED_LIMIT,nodeID);

        sql[strlen(sql)] = '\0';

        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);

        if( rc != SQLITE_OK)
        {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
}


int get_nodeID(char *streetName)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;

    sprintf(sql, "SELECT NodeID from Streets WHERE StreetName = '%s'", streetName);
    sql[strlen(sql)] = '\0';
    int rc;
    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    return atoi(data);
}


int get_path_cost(int from_nodeID, int to_nodeID)
{
    int distance;
    int incident;
    int no_cars;
    int cost;

    distance = get_distance(from_nodeID, to_nodeID);
    incident = get_incidents_points(to_nodeID);
    no_cars = get_number_of_cars_points(to_nodeID);

    return distance + incident + no_cars;
}


int get_distance(int from_nodeID, int to_nodeID)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    sprintf(sql, "SELECT Distance From Paths WHERE From_node = %d and To_node = %d;", from_nodeID, to_nodeID);
    sql[strlen(sql)] = '\0';

    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    if(strlen(data)>0)
        return atoi(data);
    else
        return INFINIT;
}


int get_incidents_points(int to_nodeID)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    sprintf(sql, "SELECT Incident FROM Streets WHERE NodeID = %d;", to_nodeID);
    sql[strlen(sql)] = '\0';

    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }

    if(atoi(data) == 0)
        return 0;
    else
        return INCIDENTS_POINTS;
}


int get_number_of_cars_points(int to_nodeID)
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    sprintf(sql, "SELECT Cars from Paths where To_node = %d;", to_nodeID);
    sql[strlen(sql)] = '\0';

    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    if(atoi(data) < NORMAL_TRAFIC)
        return atoi(data);
    else
        return INCIDENTS_POINTS;
}

void add_some_cars()
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;
    int increase;
    srand(time(NULL));
    increase = rand()%2;

    if(increase == 1)
    {
        srand(time(NULL));
        sprintf(sql, "UPDATE Paths SET Cars = Cars+%d;", rand()%NORMAL_TRAFIC);
        sql[strlen(sql)] = '\0';
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);

        if( rc != SQLITE_OK)
        {
                printf("SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }
    }
    else
    {
        srand(time(NULL));
        sprintf(sql, "UPDATE Paths SET Cars = Cars-%d;", rand()%get_minim_cars());
        sql[strlen(sql)] = '\0';
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
        fflush(stdout);

        if( rc != SQLITE_OK)
        {
                printf("SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }
    }

    //update speed limit for all

    memset(sql, 0 ,sizeof(sql));
    memset(sql, 0 ,sizeof(sql));

    sprintf(sql, "SELECT MAX(NodeID) from Streets;");
    sql[strlen(sql)] = '\0';
    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    for(int i=1; i<=atoi(data); i++)
        update_speedlimit_aglomeration(i);
}


int get_minim_cars()
{
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    int rc;

    srand(time(NULL));
    sprintf(sql, "SELECT MIN(Cars) FROM Paths;");
    sql[strlen(sql)] = '\0';
    memset(data, 0, sizeof(data));
    rc = sqlite3_exec(db, sql, callback_without_name, data, &zErrMsg);
    fflush(stdout);

    if( rc != SQLITE_OK)
    {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
    }
    return atoi(data);
}


void initializare_matrice_costuri(int Graph[][NODES])
{
    for(int i=1;i<=NODES;i++)
    {
        for(int j=1;j<=NODES; j++)
            {Graph[i][j] = get_path_cost(i,j);
            }
    }
}


int dijkstra(int Graph[][NODES], int source, int target, int path[])
{
    initializare_matrice_costuri(Graph);

    memset(path, 0, sizeof(int)*NODES);

    int dist[NODES] = {0}, prev[NODES] = {0}, selected[NODES] = {0},min, start,d,m;

    for(int i = 1;i<NODES;i++)
    {
        dist[i] = INFINIT;
        prev[i] = -1;
    }

    start = source;
    selected[start] = 1;
    dist[start] = 0;

    while(selected[target] == 0)
    {
        min = INFINIT;
        m = 0;
        for(int i=1;i < NODES;i++)
        {
            d = dist[start] + Graph[start][i];
            if(d < dist[i] && selected[i] == 0)
            {
                dist[i] = d;
                prev[i] = start;
            }
            if(min > dist[i] && selected[i] == 0)
            {
                min = dist[i];
                m = i;
            }
        }
        start = m;
        selected[start] = 1;
    }
    start = target;
    int j = 0;
    path[j++] = source;
    while(start != -1)
    {
        path[j++] = start;
        start = prev[start];
    }

    int path_reverse[NODES];
    int k;
    for(int i=j;i>0;i--,k++)
        path_reverse[k] = path[i];

    for(int i=1;i<j;i++)
        path[i] = path_reverse[i];
    return j-2;
}
