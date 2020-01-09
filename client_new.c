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
volatile int init_pozitie = 0;

char locatii_posibile[10][MSGSIZE] = {"Cuza Voda", "Ioan Cuza", "Garii", "Moara de Foc", "Traian", "Noiembrie", "Bucsinescu", "Aleea Basarabi", "Lapusneanu"};
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
void send_stop();
void check_if_car_started();

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

//comenzi trimise
const char login[] = "LOG";
const char sign_in[] = "SIG";
const char traffic_info[] = "TRF";
const char update_settings[] = "SET";
const char locatie[] = "LOC";
const char update_speed[] = "SPD";
const char delog[] = "DLG";
const char quit[] = "QUI";
const char help[] = "HEL";
const char end_path[] = "LOF";
const char destinatie[] = "DST";
const char locatie_fav[] = "FAV";
const char stopped[] = "STP";

//comenzi care pot fi introduse
const char login_display[] = "Login\n";
const char sign_display[] = "Sign in\n";
const char help_display[] = "Help\n";
const char traffic_info_display[] = "Trafic info\n";
const char update_settings_display[] = "Update settings\n";
const char destinatie_display[] = "Destinatie\n";
const char quit_display[] = "Quit\n";
const char log_out_display[] = "Log out\n";
const char locatie_fav_display[] = "Locatie favorita\n";
const char meniu_dispay[] = "Meniu\n";

//raspunsuri primite
const char raspuns_stop[] = "STP";
const char raspuns_login[] = "LOK";
const char raspuns_destinatie[] = "DSE";
const char raspuns_sing_in[] = "IOK";
const char raspuns_quit[] = "QUI";
const char raspuns_trafic_info[] = "TRF";
const char raspuns_news[] = "NEW";
const char raspuns_viteza[] = "SPD";
const char raspuns_limita[] = "LOC";
const char raspuns_final[] = "LOF";
const char raspuns_utilizatori[] = "UTL";
const char raspuns_log_out[] = "DLG";

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

void send_stop()
{
  char msg_de_trimis[MSGSIZE];
  init_pozitie = 0;
  memset(msg_de_trimis, 0, sizeof(msg_de_trimis));
  sprintf(msg_de_trimis, "%s", stopped);
  msg_de_trimis[strlen(msg_de_trimis)] = '\0';
  send_function(msg_de_trimis);
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
  while(strcmp(locatie_start, locatie_stop) == 0)
  {
    memset(locatie_stop, 0, sizeof(locatie_stop));
    strcpy(locatie_stop, locatii_posibile[rand()%9]);
    locatie_stop[strlen(locatie_stop)] = '\0';
    strcat(locatie_stop, "\n");
  }


  strcat(msg_de_trimis, locatie_start);
  strcat(msg_de_trimis, locatie_stop);

  msg_de_trimis[strlen(msg_de_trimis)] = '\0';

  send_function(msg_de_trimis);
  init_pozitie = 1;
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
  locatie_start[strlen(locatie_start)-1] = '\0';
  strcat(locatie_start, "\n");
  memset(locatie_stop, 0, sizeof(locatie_stop));

  strcpy(locatie_stop, locatii_posibile[rand()%9]);
  locatie_stop[strlen(locatie_stop)] = '\0';
  strcat(locatie_stop, "\n");

  strcpy(locatie_stop, locatii_posibile[rand()%9]);
  locatie_stop[strlen(locatie_stop)] = '\0';
  strcat(locatie_stop, "\n");
  while(strcmp(locatie_start, locatie_stop) == 0)
  {
    memset(locatie_stop, 0, sizeof(locatie_stop));
    strcpy(locatie_stop, locatii_posibile[rand()%9]);
    locatie_stop[strlen(locatie_stop)] = '\0';
    strcat(locatie_stop, "\n");
  }
  strcat(msg_de_trimis, locatie_start);
  strcat(msg_de_trimis, locatie_stop);

  msg_de_trimis[strlen(msg_de_trimis)] = '\0';
  send_function(msg_de_trimis);
  init_pozitie = 1;
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
  }

  msg_de_trimis[strlen(msg_de_trimis)] = '\0';

  if(write(sock_d, msg_de_trimis, lungime_int) <= 0)
  {
    perror("[client] Mesajul cu date NU a fost trimis");
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
    if(logat == 1 && init_pozitie == 1)
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
    else if(init_pozitie == 0 && logat==1)
    {
      check_if_car_started();
    }

  }
}


void check_if_car_started()
{
  while(init_pozitie == 0)
  {
    if((double)rand()/RAND_MAX < 0.5)
    {
      continue;
    }
    else
    {
      init_pozitie = 1;
      initializare_locatie();
      return;
    }
    sleep(30);
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
      perror("[client] Eroare la citirea lungimii din server");
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
    }

    if(strstr(msg_primit, raspuns_login)  != NULL)
    {
      logat = 1;
      main_meniu();
      if((double)rand()/RAND_MAX < 0.5)
         send_stop();
      else
         initializare_locatie();

    }

    else if(strstr(msg_primit, raspuns_stop))
    {
      printf("%s\n", KRED);
      printf("NU SE POATE OBTINE LOCATIA------------------>\n");
      printf("%s\n", KNRM);
    }

    else if(strstr(msg_primit, raspuns_destinatie) != NULL)
    {
      strcpy(locatie_stop, msg_primit+4);
      continue;
    }
    else if(strstr(msg_primit, raspuns_sing_in) != NULL)
    {
      logat = 1;
      main_meniu();
      if((double)rand()/RAND_MAX < 0.5)
      send_stop();
      else
      initializare_locatie();
    }
    else if(strstr(msg_primit, raspuns_quit) != NULL)
    {
      cancel = 1;
      close(sock_d);
    }
    else if(strstr(msg_primit, raspuns_trafic_info) != NULL)
    {
      printf("%s\n", KRED);
      printf("AVERTISMENT------------------>\n");
      printf("%s\n", KNRM);
    }
    else if(strstr(msg_primit, raspuns_news) != NULL)
    {
      printf("%s\n", KBLU);
      printf("NEWS------------------------------->\n");
      printf("%s\n", KNRM);
    }
    else if(strstr(msg_primit, raspuns_viteza) != NULL)
    {
      printf("%s\n", KRED);
      printf("AVERTISMENT VITEZA------------------>\n");
      printf("%s\n", KNRM);
    }
    else if(strstr(msg_primit, raspuns_limita) != NULL)
    {
      printf("%s\n", KCYN);
      printf("VITEZA MAXIMA ADMISA---------------->\n");
      printf("%s\n", KNRM);
    }
    else if(strstr(msg_primit, raspuns_final) != NULL)
    {
      if((double)rand()/RAND_MAX < 0.5)
         send_stop();
      else
        reinitializare_locatie(msg_primit+3);
    }
    else if(strstr(msg_primit, raspuns_utilizatori) !=NULL)
    {
      printf("%s\n", KCYN);
      printf("Numarul de utilizatori---------------->\n");
      printf("%s\n", KNRM);
    }
    else if(strstr(msg_primit, raspuns_log_out) != NULL)
    {
      logat = 0;
    }
    if(stop == 0)
        break;
    msg_primit[strlen(msg_primit)] = '\0';
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
    if( strcmp(command, login_display) == 0 && logat == 0)
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
    else if(strcmp(command, login_display) == 0 && logat == 1)
    {
      strcat(command, login);
      strcat(command, "\n");
      command[strlen(command)] = '\0';
      send_function(command);
    }
    else if(strcmp(command, sign_display) == 0 && logat == 0)
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
    else if(strcmp(command, sign_display) == 0 && logat == 1)
    {
      strcat(command, sign_in);
      strcat(command, "\n");
      command[strlen(command)] = '\0';
      send_function(command);
    }
    else if(strcmp(command, destinatie_display) == 0 && logat == 1)
    {
      char dest[MSGSIZE];
      flockfile(stdout);
      printf("Introduceti numele strazii pe care doriti sa ajungeti:\n");
      fgets(dest, sizeof(dest), stdin);
      funlockfile(stdout);
      memset(command, 0, sizeof(command));
      strcat(command, destinatie);
      strcat(command, "\n");
      strcat(command, dest);
      command[strlen(command)] = '\0';
      send_function(command);
    }
    else if(strcmp(command,update_settings_display) == 0 && logat == 1)
    {
      memset(option, 0, sizeof(option));
      fflush(stdout);
      flockfile(stdout);
      printf("Doriti sa va abonati la newsletter(Yes/No)?\n");
      fgets(option, sizeof(option), stdin);
      while(strstr(option, "Yes") == NULL && strstr(option, "No") == NULL)
      {
        memset(option, 0, sizeof(option));
        printf("Introduceti (Yes/No)\n");
        fgets(option, sizeof(option), stdin);
      }
      funlockfile(stdout);
      memset(command, 0 , sizeof(command));
      strcat(command, update_settings);
      strcat(command, option);
      command[strlen(command)] = '\0';

      send_function(command);
    }
    else if(strcmp(command, traffic_info_display) == 0 && logat == 1)
    {
      char tip_eveniment[MSGSIZE];
      memset(tip_eveniment,0, sizeof(tip_eveniment));
      memset(incident, 0, sizeof(incident));
      fflush(stdout);
      flockfile(stdout);
      printf("Introduceti un incident din trafic, specifincand strada pe care are acesta loc\n");
      printf("Alegeti tipul de eveniment:\n");
      printf("Accident\n");
      printf("Aglomeratie              Trafic normal\n");
      read(0, tip_eveniment, sizeof(tip_eveniment));
      printf("Introduceti numele strazii pe care are loc evenimentul:\n");
      read(0,incident, sizeof(incident));
      funlockfile(stdout);
      memset(command, 0 , sizeof(command));
      strcat(command, traffic_info);
      strcat(command, "\n");
      strcat(command, tip_eveniment);
      strcat(command, "\n");
      strcat(command, incident);

      command[strlen(command)] = '\0';
      send_function(command);
    }
    else if(strcmp(command, locatie_fav_display) == 0)
    {
      char tip_locatie[MSGSIZE];
      char locatie_favorita[MSGSIZE];

      memset(tip_locatie, 0, sizeof(tip_locatie));
      memset(locatie_favorita, 0, sizeof(locatie_favorita));

      fflush(stdout);
      flockfile(stdout);
      printf("Setati o strada drept locatie predefinita");
      printf("Alegeti tipul de locatie:\n");
      printf("Home                   Work\n");
      read(0, tip_locatie, sizeof(tip_locatie));
      printf("Introduceti numele strazii de care este situata locatia:\n");
      read(0, locatie_favorita, sizeof(locatie_favorita));
      funlockfile(stdout);
      memset(command, 0 , sizeof(command));
      strcat(command, locatie_fav);
      strcat(command, "\n");
      strcat(command, tip_locatie);
      strcat(command, "\n");
      strcat(command, locatie_favorita);

      command[strlen(command)] = '\0';
      send_function(command);

    }
    else if(strcmp(command, log_out_display) == 0)
    {
      fflush(stdout);

      memset(command, 0 , sizeof(command));
      strcat(command, delog);
      command[strlen(command)] = '\0';
      send_function(command);
    }
    else if(strcmp(command, quit_display) == 0)
    {
      cancel = 1;
      memset(command, 0 , sizeof(command));
      strcat(command, quit);
      command[strlen(command)] = '\0';
      send_function(command);
    }
    else if(strcmp(command,meniu_dispay) ==0 && logat == 1)
    {
      main_meniu();
    }
    else if(strcmp(command, help_display) == 0)
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
  printf("Alegeti o destinatie(Destinatie)\n");
  printf("Trimite un eveniment din trafic (Trafic info)\n");
  printf("Introduceti locatii favorite(Locatie favorita)\n");
  printf("Abonare la news(Update settings)\n");
  printf("Log out\n");
  printf("\n");
  printf("Quit\n");
  printf("%s\n", KNRM);
  printf("----------------------------------------------->\n");
}
