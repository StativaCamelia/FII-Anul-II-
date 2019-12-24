#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "sqlite3.h"

#define PORT 2019 //PORTUL FOLOSIT
#define MSGSIZE 500
#define MAXCLIENTS 100
//baza de date
sqlite3* db;

//structura folosita de clienti
static int uid = 10;

typedef struct
{
int sock_id;
char ip[INET_ADDRSTRLEN];
int UID;
} client_info;

//lista de clienti
client_info *clients[MAXCLIENTS];

//functii:
void add_client(client_info * cl);
void delete_client(int sock_id);
void send_message_to_all(char* message);
void send_msg(char *msg_trimis);
void recv_msg(void *argc);
int pregatire_raspuns(char *primit, char *raspuns, int logat, char* username);
int functie_login(char* primit,char *raspuns, int logat, char* username);
void get_user_and_pass(char* msg_primit, char* username, char* pass);
int functie_sign_in(char* msg_primit, char* raspuns, int logat, char* username);
void functie_help_login(char* raspuns);
void functie_help_main(char* raspuns);


extern int errno;

void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

static int callback_insert(void *data, int argc, char **argv, char **azColName) {
   int i;
   char *ans = (char*) data;
   for(i = 0; i<argc; i++) {
      sprintf(ans,"%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   return 0;
}

static int callback(void *data, int argc, char **argv, char **azColName)
{
   int i;
   char *ans = (char*)data;
   for(i = 0; i<argc; i++)
   {
      sprintf(ans,"%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   strcat(ans, "\n");
   return 0;
}

//Ce comenzi poate sa primeasca serverul:
    char login[] = "Login";
    char sign_in[] = "Sign in";
    char traffic_info[] = "Trafic info";
    char update_settings[] = "Update Settings";
    char update_location[] = "Update Location";
    char update_speed[] = "Update Speed"; 
    char quit[] = "Quit";
    char help[] = "Help";


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
   
    //ip-ul clientilor
    char ip[INET_ADDRSTRLEN];
   
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
        //adaugam clientul la lista actuala de clienti
        client_info *cl = (client_info*)malloc(sizeof(client_info));
   
        //client uid and socket
        dup2(sock_client,uid);
        cl->sock_id = uid;
        cl->UID = uid;
        uid++;
        //convertim addresa clientului intr-un string
        inet_ntop(AF_INET, (struct sockaddt*)&from, ip, INET_ADDRSTRLEN);
        //adaugam ip-ul clientului
        strcpy(cl->ip,ip);
        
        //adaugam socketul clientului
        cl->sock_id = sock_client;
        add_client(cl);

        //handler pentru client
        int pid = fork();
        
        if(pid == 0)
        {   
            fflush(stdout);
            fflush(stdin);

            close(sock_serv);
            int logat = 0;
            char locatie[3];
            char viteza[3];
            int cancel = 0;

            bzero(&locatie,sizeof(locatie));
            bzero(&viteza, sizeof(viteza));

            char lungime_str[3];
            int lungime_int = 0;
            int stop;

            while(cancel != 1)
            {
                fflush(stdout);
                fflush(stdin);

                bzero(lungime_str, sizeof(lungime_str));
        
                if((stop=read(uid-1, lungime_str, sizeof(lungime_str)))<0)
                {
                    perror("[server] Eroare la citirea lungimii in server");
                    return errno;
                }
                if(stop == 0)
                    break;
                lungime_int = atoi(lungime_str);
                char *msg_primit = (char*)malloc(lungime_int);
                memset(msg_primit, 0, sizeof(msg_primit));
                if(read(uid-1, msg_primit, lungime_int+1)<0)
                {
                    perror("[server] Eroare la citirea mesajului in server");
                    return errno;
                } 
                if(stop == 0)
                    break;
                printf("Mesaj primit:%s,%d\n", msg_primit, strlen(msg_primit));
                //trimitere mesaje:

                char msg_de_trimis[MSGSIZE];
                char username[100];
                bzero(&msg_de_trimis, sizeof(msg_de_trimis));
                if(strstr(msg_primit, quit) != NULL)
                {
                    cancel = 1;
                    sprintf(msg_de_trimis, "QUI: Urmeaza sa va deconectati...");
                }
                else
                {
                    logat = pregatire_raspuns(msg_primit, msg_de_trimis, logat, username);
                }

                msg_de_trimis[strlen(msg_de_trimis)] = '\0';
                lungime_int = strlen(msg_de_trimis);
                bzero(&lungime_str, sizeof(lungime_str));
                sprintf(lungime_str,"%d", lungime_int);           
                
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
                if(cancel == 1)
                {
                    close(uid-1);
                    break;
                }
            }//end while(1)
            exit(1);
        }//end if fork
        close(sock_client);
        close(uid-1);
    }
    sqlite3_close(db);
}
    //adaug un client la lista actuala de clienti conectati la server
    void add_client(client_info * cl)
    {   
        for(int i = 0; i< MAXCLIENTS; ++i)
        {   
            if(!clients[i])
            {  
                clients[i] = cl;
                break;
            }
        }
    }
    //elimin un client din lista actuala de clienti conectati la server
    void delete_client(int sock_id)
    {
        for(int i=0; i < MAXCLIENTS; i++)
        {
            if(clients[i])
            {
                if(clients[i]->sock_id == sock_id)
                {
                    clients[i] = NULL;
                    break;
                }
            }
        }
    }

void send_message_to_all(char* message)
{   
    char *send = (char*) malloc((strlen(message)+5)*sizeof(char));
    strcpy(send,"INFO:");
    strcat(send,message);
    for(int i=0; i<MAXCLIENTS;i++)
    {   
        if(clients[i])
        {   
            if(write(clients[i]->UID, send, strlen(send)) < 0)
            {
                perror("[server] Eroare la scriere in send_to_all");
                break;
            }
        
        }    
    }
}

int pregatire_raspuns(char *msg_primit, char *msg_de_trimis, int logat, char* username)
{
    if(strstr(msg_primit, login) != NULL)
    {
        logat = functie_login(msg_primit, msg_de_trimis, logat, username);
    }
    else if(strstr(msg_primit, sign_in) != NULL)
    {
        logat = functie_sign_in(msg_primit, msg_de_trimis, logat, username);
    }
    else if(strstr(msg_primit, help) != NULL && logat == 0)
    {
        logat = logat;
        functie_help_login(msg_de_trimis);
    }
    else if(strstr(msg_primit, help) != NULL && logat == 1)
    {
        logat = logat;
        functie_help_main(msg_de_trimis);
    }
    else 
    {   
        sprintf(msg_de_trimis, "ERR: Comanda introdusa nu exista");
        logat = logat;
    }
    return logat;
}


void functie_help_login(char* msg_de_trimis)
{
    strcat(msg_de_trimis, "Pentru Login introduce comanda <Login>, ulterior vi se vor solicita username-ul si parola, asociate contului\n");
    strcat(msg_de_trimis, "Pentru sign_in ntorduceti comanda <Sign_in>, ulteror vi se vor solicita username-ul si parola noului cont\n");
    strcat(msg_de_trimis, "Pentru a parasi aplicatie introduceti comanda <Quit>");
}

void functie_help_main(char* msg_de_trimis)
{
    strcat(msg_de_trimis, "Aplicatie este destinata monitorizarii traficului, drept urmare pozitia respectiv viteza dumneaavoastra sunt utilizate in acest scop\n");
    strcat(msg_de_trimis, "In acest fel urmeaza a fi instintati cu privire la evenimentele petrecute in trafic si respectiv cu privire la restrictiile de viteza\n");
    strcat(msg_de_trimis, "Daca doriti raportarea unui eveniment introduceti comanda <Trafic Info>, urmata de mesajul dumneavoastra");
    strcat(msg_de_trimis, "Daca doriti sa va abonati la stirile legate de vreme, statii peco, etc, introduceti comanda <Settings Update>");
}


void get_user_and_pass(char* msg_primit, char *username, char *pass)
{

    memset(username, 0, sizeof(username));
    memset(pass, 0, sizeof(pass));
    char copy[MSGSIZE];
    strcpy(copy, msg_primit);
    char *ptr = strtok(copy, "\n");
    ptr = strtok(NULL, "\n");
    sprintf(username, "%s", ptr);
    ptr = strtok(NULL, "\n");
    sprintf(pass,"%s", ptr);
}

int functie_sign_in(char* msg_primit, char *msg_de_trimis, int logat, char *username)
{
    char pass[100];
    char sql[MSGSIZE];
    char data[MSGSIZE];
    data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;

    if(logat == 1)
    {
        sprintf(msg_de_trimis, "INU:Utilizatorul %s este deja logat", username);
        logat = logat;
    }
    else
    {
        get_user_and_pass(msg_primit, username, pass);
        //trebuie verificat daca exista sau nu
        sprintf(sql, "SELECT Username FROM Clienti");
        memset(data, 0, sizeof(data));
        int rc;
        rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);
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
            sql[0] = 0;
            sprintf(sql, "INSERT INTO Clienti (Username, Password, Options) VALUES('%s','%s',0);",username, pass);
            memset(data, 0, sizeof(data));
            data[0] = 0;
            rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);
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
    }
    fflush(stdout);
    fflush(stdin);
    return logat;
}


int functie_login(char *msg_primit, char *msg_de_trimis, int logat, char*username)
{   
    char pass[MSGSIZE];
    char sql[MSGSIZE];
    char data[MSGSIZE];
    //data[0] = 0;
    sql[0] = 0;
    char *zErrMsg = 0;
    if(logat == 1)
    {
        sprintf(msg_de_trimis, "Utilizatorul %s este deja logat\n", username);
        logat = logat;
    }
    else
    {   
        get_user_and_pass(msg_primit, username, pass);
        sprintf(sql, "SELECT * from Clienti where Username = '%s'",username);
        int rc;
        memset(msg_de_trimis, 0 , sizeof(msg_de_trimis));
        memset(data, 0, sizeof(data));
        rc = sqlite3_exec(db, sql, callback, data, &zErrMsg);
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
            sprintf(msg_de_trimis, "LNU:Username-ul %s nu exista\n", username);
            logat = 0;
        }
    }
    return logat;
    fflush(stdout);
    fflush(stdin);
}
