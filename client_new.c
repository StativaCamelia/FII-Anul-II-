#include <sys/types.h>
#include <sys/socket.h>
#include <termios.h>
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
#define KBLU  "\x1B[34m"
#define KNRM  "\x1B[0m"
#define KCYN  "\x1B[36m"
#define KRED  "\x1B[31m"

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

//variabila care anunta inchiderea
volatile int cancel = 0;
volatile int logat = 0;
volatile int locatie_ok = 0;
char locatii_posibile[10][MSGSIZE] = {"Cuza Voda", "Alexandru Ioan Cuza", "Garii", "Moara de Foc", "Traian", "Noiembrie", "Bucsinescu", "Aleea Basarabi", "Lapusneanu"};
char locatie_start[MSGSIZE];
char locatie_stop[MSGSIZE];

//FUCTII
void print_meniu();

void *receive_function(void *arg);
void *speed_update(void *arg);
void *commands_send(void *arg);
void *location_update(void *arg);
void initializare_locatie();
void reinitializare_locatie();

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
//comenzi
char login[] = "LOG";
char sign_in[] = "SIG";
char traffic_info[] = "TRF";
char update_settings[] = "SET";
char locatie[] = "LOC";
char update_speed[] = "SPD";
char delog[] = "DLG";
char quit[] = "QUI";
char help[] = "HEL";
char end_path[] = "LOF";

int getch()
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}


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

        pthread_t speed_thread, recv_thread, command_thread;
        if(pthread_mutex_init(&lock, NULL))
        {
            printf("Initializarea mutex nu a reusit");
        }

        if(pthread_mutex_init(&start, NULL))
        {
            printf("Initializarea mutex nu a reusit");
        }
        fflush(stdout);

        pthread_create(&recv_thread, NULL, &receive_function, NULL);
        pthread_create(&speed_thread, NULL, &speed_update, NULL);
        pthread_create(&command_thread, NULL, &commands_send, NULL);

        pthread_join(command_thread, NULL);
        pthread_join(speed_thread, NULL);
        pthread_join(recv_thread, NULL);

        pthread_mutex_destroy(&lock);
        exit(1);
}


void initializare_locatie()
{

    char msg_de_trimis[MSGSIZE];

    fflush(stdout);

    memset(locatie_start, 0, sizeof(locatie_start));
    memset(locatie_stop, 0, sizeof(locatie_stop));
    memset(msg_de_trimis, 0, sizeof(msg_de_trimis));

    strcat(msg_de_trimis, "LOC");
    strcat(msg_de_trimis, "\n");
    srand(time(NULL));

    strcpy(locatie_start, locatii_posibile[rand()%9]);
    locatie_start[strlen(locatie_start)] = '\0';
    strcat(locatie_start, "\n");

    strcpy(locatie_stop, locatii_posibile[rand()%9]);
    locatie_stop[strlen(locatie_stop)] = '\0';
    strcat(locatie_stop, "\n");

    strcat(msg_de_trimis, locatie_start);
    strcat(msg_de_trimis, locatie_stop);

    msg_de_trimis[strlen(msg_de_trimis)] = '\0';

    send_function(msg_de_trimis);
    locatie_ok = 1;
}


void reinitializare_locatie(char *locatie)
{
  char msg_de_trimis[MSGSIZE];

  fflush(stdout);

  memset(locatie_start, 0, sizeof(locatie_start));
  memset(msg_de_trimis, 0, sizeof(msg_de_trimis));

  strcat(msg_de_trimis, "LOC");
  strcat(msg_de_trimis, "\n");
  srand(time(NULL));

  strcpy(locatie_start, locatie_stop);
  locatie_start[strlen(locatie_start)] = '\0';
  strcat(locatie_start, "\n");
  memset(locatie_stop, 0, sizeof(locatie_stop));

  strcpy(locatie_stop, locatii_posibile[rand()%9]);
  locatie_stop[strlen(locatie_stop)] = '\0';
  strcat(locatie_stop, "\n");

  strcat(msg_de_trimis, locatie_start);
  strcat(msg_de_trimis, locatie_stop);

  msg_de_trimis[strlen(msg_de_trimis)] = '\0';

  send_function(msg_de_trimis);
  locatie_ok = 1;
}


void send_function(char *msg_de_trimis)
{
    int lungime_int;
    char lungime_str[3];
    pthread_mutex_lock(&lock);
    lungime_int = strlen(msg_de_trimis);
    fflush(stdout);

    sprintf(lungime_str,"%d", lungime_int);

    if(write(sock_d, lungime_str, sizeof(lungime_str)) <= 0)
    {
        perror("[client] Mesajul cu lungimea NU a fost trimis");
        return errno;
    }

    msg_de_trimis[strlen(msg_de_trimis)] = '\0';

    if(write(sock_d, msg_de_trimis, lungime_int) <= 0)
    {
        perror("[client] Mesajul cu date NU a fost trimis");
        return errno;
    }
    pthread_mutex_unlock(&lock);
}


void *speed_update(void *arg)
{
    char viteza_str[MSGSIZE] = {0};
    char msg[MSGSIZE] = {0};
    int viteza_int = 0;

    while(cancel == 0)
        {
            fflush(stdout);
            if(logat == 1 && locatie_ok == 1)
            {
                fflush(stdout);

                memset(viteza_str, 0, sizeof(viteza_str));
                memset(msg, 0, sizeof(msg));

                srand(time(NULL));
                viteza_int = 10 + rand()%(121-10);

                sprintf(viteza_str, "%d", viteza_int);
                strcat(msg, "SPD");
                strcat(msg, viteza_str);
                strcat(msg, "\n");
                msg[strlen(msg)] = '\0';
                send_function(msg);
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
            initializare_locatie();

        }
        else if(strstr(msg_primit, "IOK") != NULL)
        {
            logat = 1;
            main_meniu();
            initializare_locatie();
        }
        else if(strstr(msg_primit, "QUI") != NULL)
        {
            cancel = 1;
            close(sock_d);
        }
        else if(strstr(msg_primit, "TRF") != NULL)
        {
            printf("%s\n", KRED);
            printf("AVERTISMENT------------------>\n");
            printf("%s\n", KNRM);
        }
        else if(strstr(msg_primit, "NEW") != NULL)
        {
            printf("%s\n", KBLU);
            printf("NEWS------------------------------->\n");
            printf("%s\n", KNRM);
        }
        else if(strstr(msg_primit, "SPD") != NULL)
        {
            printf("%s\n", KRED);
            printf("AVERTISMENT VITEZA------------------>\n");
            printf("%s\n", KNRM);
        }
        else if(strstr(msg_primit, "LOC") != NULL)
        {
            printf("%s\n", KCYN);
            printf("VITEZA MAXIMA ADMISA---------------->\n");
            printf("%s\n", KNRM);
        }
        else if(strstr(msg_primit, "LOF") != NULL)
        {
          reinitializare_locatie(msg_primit+3);
        }
        else if(strstr(msg_primit, "DLG") != NULL)
        {
            logat = 0;
        }
        if(stop == 0)
            break;
        printf("\n%s\n", msg_primit+4);
    }
}


void *commands_send(void *arg)
{
    char command[MSGSIZE];
    char username[MSGSIZE];
    char password[MSGSIZE];
    char option[MSGSIZE];
    char incident[MSGSIZE];

    while(cancel == 0)
    {
        memset(command, 0, sizeof(command));
        fflush(stdout);

        read(0, command, sizeof(command));

        if(strstr(command, "Login") != NULL && logat == 0)
        {
            memset(username, 0, sizeof(username));
            memset(password, 0, sizeof(password));
            fflush(stdout);
            printf("Introduceti numele utilizatorului:");
            fgets(username, sizeof(username), stdin);

            fflush(stdout);
            printf("Introduceti parola utilizatorului:");
            int p = 0;
            char pass[1];
            do{
                password[p] = getch();
                if(password[p] != '\n'){
                printf("*");
            }
            p++;
            }while(password[p-1]!='\n');

            memset(command, 0 , sizeof(command));
            strcat(command, login);
            strcat(command, "\n");
            strcat(command, username);
            strcat(command, password);
            command[strlen(command)] = '\0';
            send_function(command);
        }
        else if(strstr(command, "Sign in") != NULL && logat == 0)
        {
            memset(username, 0, sizeof(username));
            memset(password, 0, sizeof(username));

            fflush(stdout);
            printf("Alegeti un nume de utilizator:");
            fgets(username, sizeof(username), stdin);

            fflush(stdout);
            printf("Introduceti parola noului cont:");
            int p1=0;
            do{
                password[p1]=getch();
                if(password[p1]!='\n')
                {
                    printf("*");
                }
            p1++;
            }while(password[p1-1]!='\n');
            memset(command, 0 , sizeof(command));
            strcat(command, sign_in);
            strcat(command,"\n");
            strcat(command, username);
            strcat(command, password);
            command[strlen(command)] = '\0';
            printf("\n");
            send_function(command);
        }
        else if(strstr(command,"Update Settings") != NULL && logat == 1)
        {
            memset(option, 0, sizeof(option));
            fflush(stdout);
            printf("Doriti sa va abonati la newsletter(Yes/No)?\n");
            fgets(option, sizeof(option), stdin);
            while(strstr(option, "Yes") == NULL && strstr(option, "No") == NULL)
            {
                memset(option, 0, sizeof(option));
                printf("Introduceti (Yes/No)\n");
                fgets(option, sizeof(option), stdin);
            }
            memset(command, 0 , sizeof(command));
            strcat(command, update_settings);
            strcat(command, option);
            command[strlen(command)] = '\0';

            send_function(command);
        }
        else if(strstr(command, "Trafic info") != NULL && logat == 1)
        {
            char tip_eveniment[MSGSIZE];
            memset(tip_eveniment,0, sizeof(tip_eveniment));
            memset(incident, 0, sizeof(incident));
            fflush(stdout);
            printf("Introduceti un incident din trafic, specifincand strada pe care are acesta loc\n");
            printf("Alegeti tipul de eveniment:\n");
            printf("Radar                    Accident\n");
            printf("Aglomeratie              Trafic normal\n");
            read(0, tip_eveniment, sizeof(tip_eveniment));
            printf("Introduceti numele strazii pe care are loc evenimentul:\n");
            read(0,incident, sizeof(incident));
            memset(command, 0 , sizeof(command));
            strcat(command, traffic_info);
            strcat(command, "\n");
            strcat(command, tip_eveniment);
            strcat(command, "\n");
            strcat(command, incident);
            command[strlen(command)] = '\0';

            printf("%s", command);
            send_function(command);
        }
        else if(strstr(command, "Log out") != NULL && logat == 1)
        {
            fflush(stdout);

            memset(command, 0 , sizeof(command));
            strcat(command, delog);
            command[strlen(command)] = '\0';
            send_function(command);
        }
        else if(strstr(command, "Quit") != NULL)
        {
            cancel = 1;
            memset(command, 0 , sizeof(command));
            strcat(command, quit);
            command[strlen(command)] = '\0';
            send_function(command);
        }
        else if(strstr(command, "Help") != NULL)
        {
          memset(command, 0 , sizeof(command));
          strcat(command, help);
          command[strlen(command)] = '\0';
          send_function(command);
        }
        else{
            fflush(stdout);
            command[strlen(command)] = '\0';
            send_function(command);
            }
    }
}

void print_meniu()
{
     //---------------->MENIU<------------------
    printf("\n---------------------------------------------->\n");
    printf("%s\n", KBLU);
    printf("Bine ati venit in aplicatia pentru Monitorizarea Traficului!\n");
    printf("%s\n", KNRM);
    printf("Selectati una din optiunile urmatoare pentru a putea incepe:\n");
    printf("%s\n", KCYN);
    printf("Login\n");
    printf("Sign in\n");
    printf("Help\n");
    printf("\n");
    printf("%s\n", KRED);
    printf("Ouit\n");
    printf("%s\n", KNRM);
    printf("---------------------------------------------->\n");
}

void main_meniu()
{
    printf("\n---------------------------------------------->\n");
    printf("%s\n", KCYN);
    printf("Trimite un eveniment din trafic (Trafic info)\n");
    printf("Abonare la news(Update Settings)\n");
    printf("%s\n", KRED);
    printf("Deconectare\n");
    printf("\n");
    printf("Quit\n");
    printf("%s\n", KNRM);
    printf("----------------------------------------------->\n");
}
