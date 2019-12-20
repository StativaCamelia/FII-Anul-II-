#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sqlite3.h>

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
void print_clients();

void send_msg(char *msg_trimis);
void recv_msg(void *argc);

extern int errno;

void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int main(int argc, char*argv[])
{
    //Ce comenzi poate sa primeasca serverul:
    char login[] = "Login";
    char sign_in[] = "Sign in";
    char traffic_info[] = "Trafic info";
    char update_settings[] = "Update Settings";
    char update_location[] = "Update Location";
    char update_speed[] = "Update Speed"; 
    char quit[] = "QUIT";
    char help[] = "Help";

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
            int ok_logat = 0;
            char locatie[3];
            char viteza[3];
            int cancel = 0;

            bzero(&locatie,sizeof(locatie));
            bzero(&viteza, sizeof(viteza));

            char lungime_str[3];
            int lungime_int = 0;
            int stop;
            while(1)
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
                
                if(read(uid-1, msg_primit, lungime_int+1)<0)
                {
                    perror("[server] Eroare la citirea mesajului in server");
                    return errno;
                } 
                if(stop == 0)
                    break;
                printf("Mesaj primit:%s\n",msg_primit);
                //trimitere mesaje:
                //---> o functie care proceseaza mesajul primit
                
                char msg_de_trimis[] = "BUNA";
                lungime_int = strlen(msg_de_trimis);
                bzero(&lungime_str, sizeof(lungime_str));
                sprintf(lungime_str,"%d", lungime_int-1);
                
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
            }//end while(1)
            exit(1);
        }//end if fork
        close(sock_client);
        close(uid);
    }
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
