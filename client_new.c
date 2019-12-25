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
volatile int logat = 0;
volatile int viteza_int = 0;
volatile int start_location = 0;

//FUCTII
void print_meniu();

void *receive_function(void *arg);
void *speed_update(void *arg);
void *commands_send(void *arg);
void *location_update(void *arg);

void send_function(char *msg_de_trimis);

void login_meniu();
void main_meniu();

//mutex
pthread_mutex_t lock =PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t start =PTHREAD_MUTEX_INITIALIZER;
//mesaje
char msg_trimis[MSGSIZE];

//sock
int sock_d;

int main(int argc, char* argv[])
{       
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

        pthread_t speed_thread, recv_thread, command_thread, location_thread;
        if(pthread_mutex_init(&lock, NULL))
        {
            printf("Initializarea mutex nu a reusit");
        }

        if(pthread_mutex_init(&start, NULL))
        {
            printf("Initializarea mutex nu a reusit");
        }

        //pthread_create(&location_thread, NULL, &location_update, NULL);
        pthread_create(&recv_thread, NULL, &receive_function, NULL);
        pthread_create(&speed_thread, NULL, &speed_update, NULL);
        pthread_create(&command_thread, NULL, &commands_send, NULL);
        
        //pthread_join(location_thread, NULL);
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
    pthread_mutex_unlock(&lock);
}

void *location_update(void* arg)
{
    char location_info[MSGSIZE];
    char start_point[MSGSIZE];
    char stop_point[MSGSIZE];
    int begin = 0;
    while(cancel == 0)
    {   
        begin = logat;
        fflush(stdin);
        fflush(stdout);
        if(begin == 1)
        {   
            printf("Introduceti locatia de start:");
            fgets(start_point,sizeof(start_point), stdin);
            printf("Introduceti destinatia:");
            fgets(stop_point, sizeof(stop_point), stdin);
            strcat(location_info, start_point);
            strcat(location_info, stop_point);
        }
    }
}


void *speed_update(void *arg)
{
    char viteza_str[MSGSIZE];
    int begin = 0;
    while(cancel == 0)
        {   
            begin = logat;
            fflush(stdin);
            fflush(stdout);
            if(begin == 1 && start_location == 1)
            {
                srand(time(NULL));
                viteza_int = rand()%70;
                sprintf(viteza_str, "SPEED : %d", viteza_int);
                //viteza_str[strlen(viteza_str)] = '\0';
                send_function(viteza_str);
                sleep(60);
            }
        }
    }


void *receive_function(void *arg)
{
    while(cancel == 0)
    {
        char lungime_str[3];
        int lungime_int;

        fflush(stdout);
        fflush(stdin);
        memset(lungime_str,0,sizeof(lungime_str));
    
        int stop;
        
        if((stop=read(sock_d, lungime_str, sizeof(lungime_str)))<0)
        {
            perror("[client] Eroare la citirea lungimii in server");
            return errno;
        }
        
        if(stop == 0)
        {
            cancel = 1;
            break;
        }
        lungime_int = atoi(lungime_str);
        char *msg_primit = (char*)malloc(lungime_int);            
        memset(msg_primit, 0, sizeof(msg_primit));
        if(read(sock_d, msg_primit, lungime_int+1)<0)
        {
            perror("[client] Eroare la citirea mesajului in server");
            return errno;
        } 

        if(strstr(msg_primit, "LOK") != NULL)
        {
            logat = 1;
            main_meniu();
        }
        else if(strstr(msg_primit, "IOK") != NULL)
        {
            logat = 1;
            main_meniu();
        }
        else if(strstr(msg_primit, "QUI") != NULL)
        {
            cancel = 1;
            close(sock_d);
        }
        if(stop == 0)
            break;
        printf("%s\n",msg_primit+4);
    }
}


void *commands_send(void *arg)
{
    char commanda[MSGSIZE];
    char username[MSGSIZE];
    char password[MSGSIZE];
    char option[MSGSIZE];
    char incident[MSGSIZE];

    while(cancel == 0)
    {
        bzero(&commanda,sizeof(commanda));
        
        fflush(stdin);
        fflush(stdout);
        
        read(0, commanda, sizeof(commanda));
        
        if(strstr(commanda, "Login") != NULL && logat == 0)
        {  
            bzero(&username, sizeof(username));
            bzero(&username, sizeof(username));
            fflush(stdout);
            fflush(stdin);
            printf("Introduceti numele utilizatorului:");
            fgets(username, sizeof(username), stdin);
            
            fflush(stdin);
            fflush(stdout);
            printf("Introduceti parola utilizatorului:");
            fgets(password, sizeof(password), stdin);

            strcat(commanda, username);
            strcat(commanda, password);
            commanda[strlen(commanda)] = '\0';
            send_function(commanda);
        }
        else if(strstr(commanda, "Sign in") != NULL && logat == 0)
        {
            bzero(&username, sizeof(username));
            bzero(&username, sizeof(username));

            fflush(stdout);
            fflush(stdin);
            printf("Alegeti un nume de utilizator:");
            fgets(username, sizeof(username), stdin);

            
            fflush(stdin);
            fflush(stdout);
            printf("Introduceti parola noului cont:");
            fgets(password, sizeof(password), stdin);

            strcat(commanda, username);
            strcat(commanda, password);
            commanda[strlen(commanda)] = '\0';
            send_function(commanda);
        }
        else if(strstr(commanda,"Update Settings") != NULL && logat == 1)
        {
            bzero(option, sizeof(option));
            fflush(stdin);
            fflush(stdout);
            printf("Doriti sa va abonati la newsletter(Yes/No)?\n");
            fgets(option, sizeof(option), stdin);
            while(strstr(option, "Yes") == NULL && strstr(option, "No") == NULL)
            {
                bzero(option, sizeof(option));
                printf("Introduceti (Yes/No)\n");
                fgets(option, sizeof(option), stdin);
            }
            strcat(commanda, option);
            commanda[strlen(commanda)] = '\0';

            send_function(commanda);
        }
        else if(strstr(commanda, "Trafic info") != NULL)
        {
            printf("Introduceti un incident din trafic, specifincand strada pe care are acesta loc");
            fgets(incident, sizeof(option), stdin);
            strcat(commanda, incident);
            send_function(commanda);
        }
        
        else if(strstr(commanda, "Quit") != NULL)
        {
            cancel = 1;
            send_function(commanda);
        }
        else{

            commanda[strlen(commanda)] = '\0';
            send_function(commanda);
        }
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

    printf("Trimite un eveniment din trafic (Trafic info)\n");
    printf("Deconectare\n");
    printf("Abonare la news(Update Settings)\n");
    printf("Quit\n");
    printf("----------------------------------------------->\n");

}
