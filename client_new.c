#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/select.h>

#define MSGSIZE 500
#define PORT 2019
/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

//variabila care anunta inchiderea
int cancel = 0;
int logat = 0;

fd_set master;
fd_set read_fds;

//FUCTII
void print_meniu();

void *receive_function(void *arg);
void *speed_update(void *arg);
void *commands_send(void *arg);

void send_function(char *msg_de_trimis);

void login_meniu();
void main_meniu();

//mutex
pthread_mutex_t lock =PTHREAD_MUTEX_INITIALIZER;

//mesaje
char msg_trimis[MSGSIZE];

//sock
int sock_d;

int main(int argc, char* argv[])
{       
        /*
        FD_ZERO(&master);
        FD_ZERO(&read_fds);

        FD_SET(0,&master);
        FD_SET(sock_d,&master);
        
        if (select(sock_d+1, &read_fds, NULL,NULL,NULL) == -1)
        {
            perror("[server]Select");
        }
        */
        //structura server
        struct sockaddr_in server;
        /* cream socketul */
        if ((sock_d = socket (AF_INET, SOCK_STREAM, 0)) == -1)
        {
            perror ("Eroare la socket().\n");
            return errno;
        }
         // umplem structura folosita pentru realizarea conexiunii cu serverul 
         //familia socket-ului 
         server.sin_family = AF_INET;
        // adresa IP a serverului 
        server.sin_addr.s_addr = inet_addr("127.0.0.1");
        // portul de conectare 
        server.sin_port = htons (PORT);
        // ne conectam la server 
       
        if (connect (sock_d, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
        {
            perror ("[client]Eroare la connect().\n");
            return errno;
        }
   
        print_meniu();

        pthread_t speed_thread, recv_thread, command_thread;
        int thread_1, thread_2;

        if(pthread_mutex_init(&lock, NULL))
        {
            printf("Initializarea mutex nu a reusit");
        }
        pthread_create(&recv_thread, NULL, &receive_function, NULL);
        pthread_create(&speed_thread, NULL, &speed_update, NULL);
        pthread_create(&command_thread, NULL, &commands_send, NULL);
        pthread_join(command_thread, NULL);
        pthread_join(speed_thread, NULL);
        pthread_join(recv_thread, NULL);
        
        pthread_mutex_destroy(&lock);
        exit(1);
}

void send_function(char *msg_de_trimis)
{
    int lungime_int;
    char lungime_str[3];
    pthread_mutex_lock(&lock);
    lungime_int = strlen(msg_de_trimis);
    sprintf(lungime_str,"%d", lungime_int-1);
    if(write(sock_d, lungime_str, sizeof(lungime_str)) <= 0)
    {
        perror("[client] Mesajul cu lungimea NU a fost trimis");
        return errno;
    }
    if(write(sock_d, msg_de_trimis, lungime_int) <= 0)
    {
        perror("[client] Mesajul cu date NU a fost trimis");
        return errno;
    }
    printf("%s", msg_de_trimis);
    pthread_mutex_unlock(&lock);
}

void *speed_update(void *arg)
{
    int viteza_int;
    char viteza_str[3];
    if(logat == 0)
    {
        while(1)
        {   fflush(stdin);
            fflush(stdout);
            srand(time(NULL));
            viteza_int = 80;
            sprintf(viteza_str, "%d", viteza_int);
            send_function(viteza_str);
            sleep(60);
        }
    }
}
void *receive_function(void *arg)
{
    while(1)
    {
        char lungime_str[3];
        int lungime_int;
        fflush(stdout);
        fflush(stdin);

        int stop;
        
        if((stop=read(sock_d, lungime_str, sizeof(lungime_str)))<0)
        {
            perror("[client] Eroare la citirea lungimii in server");
            return errno;
        }
        
        if(stop == 0)
            break;
        
        lungime_int = atoi(lungime_str);
        char *msg_primit = (char*)malloc(lungime_int);            
        
        if(read(sock_d, msg_primit, lungime_int+1)<0)
        {
            perror("[client] Eroare la citirea mesajului in server");
            return errno;
        } 
        
        if(stop == 0)
            break;
        printf("Mesaj primit:%s\n",msg_primit);
    }
}

void *commands_send(void *arg)
{
    char commanda[MSGSIZE];
    while(1)
    {
        bzero(&commanda,sizeof(msg_trimis));
        fflush(stdin);
        fflush(stdout);
        read(0, commanda, sizeof(commanda));
        send_function(commanda);
    }
}
void print_meniu()
{
     //---------------->MENIU<------------------
    printf("---------------------------------------------->\n");
    printf("SALUT!\n");
    printf("Select an option:\n");
    printf("Login\n");
    printf("Sign in\n");
    printf("Help\n");
    printf("Ouit\n");
    printf("---------------------------------------------->\n");
}

void main_meniu()
{
    printf("---------------------------------------------->\n");
    printf("Sunteti conectat ca si Utilizator1\n");
    printf("Trimite un eveniment din trafic (Trafic info)\n");
    printf("Deconectare\n");
    printf("Abonare la news(Update Settings)\n");
    printf("Quit\n");
    printf("----------------------------------------------->\n");

}