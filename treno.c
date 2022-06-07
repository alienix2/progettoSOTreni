#include <stdio.h>
#include <arpa/inet.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> 
#include <sys/stat.h>

char *itinerarioFolder = "./itinerario";
char *logFolder = "./logs";
int segmentiDescriptor[17];
int ETCS;
int terminato = 0;
sem_t *semafori[17];
char *nomiSemafori[17];

typedef struct{
    int numTreno;
    int posizioneAttuale;
    int posizioneProssima;
    char *itinerario[7];
    FILE *log;
}TRENO;

char *getTime(char *data){
    time_t tempo;
    struct tm *info;
    time(&tempo);
    info = localtime(&tempo);
    strftime(data,50,",\t%x - %X\n", info);
    return data;
}

void contaTerminazioniHandler(){
    terminato++;
}

void preliminariTerminazione(TRENO *trenoCorrente){
    for(int i = 0; i<7; i++){
        free(trenoCorrente->itinerario[i]);
    }
    fclose(trenoCorrente->log);
    if(ETCS)
    kill(getppid(), SIGUSR1);
}

void terminaProcessi(int pid[7]){
    char ma[20];
    for(int i = 1; i<17; i++){
        close(segmentiDescriptor[i]);
        sem_unlink(nomiSemafori[i]);
    }
    for(int i = 1; i<6; i++){
        kill(pid[i], SIGTERM);
    }
    if(ETCS == 2) kill(pid[6], SIGTERM);
}

void aggiungiLog(TRENO trenoCorrente, const char *fmt, ...){
    va_list args;
    char messaggioLog[100];
    char data[50];
    va_start(args, fmt);
    vsnprintf(messaggioLog, 100, fmt, args);
    va_end(args);
    strcat(messaggioLog, getTime(data));
    fputs(messaggioLog, trenoCorrente.log);
    fflush(trenoCorrente.log);
}

void creaSegmenti(){
    char ma[20];
    for(int i = 1; i<17; i++){
        sprintf(ma, "%s/MA%d.txt",itinerarioFolder, i);
        segmentiDescriptor[i] = open(ma, O_CREAT|O_RDWR);
        fchmod(segmentiDescriptor[i], S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
        if(write(segmentiDescriptor[i], "0", 1) < 0) exit(EXIT_FAILURE);
    }
}

void inizializzaSemafori(){
    char nome[10];
    for(int i = 1; i<17; i++){
        nomiSemafori[i] = (char*) malloc(strlen(nome));
        sprintf(nomiSemafori[i], "sem%d", i);
        semafori[i] = sem_open(nomiSemafori[i], O_CREAT, 0777, 1);
    }
}

void creaLog(TRENO *trenoCorrente){
    char nomeLog[20];
    sprintf(nomeLog, "%s/T%d.log",logFolder, trenoCorrente->numTreno);
    if((trenoCorrente->log = fopen(nomeLog, "w")) < 0){
        kill(getppid(), SIGUSR1);
        kill(getpid(), SIGKILL);
        exit(EXIT_FAILURE);
    }
}

void creaSocket(int *server, struct sockaddr_un *serverAddress, int *serverLen, char* nomeServer){
    *serverLen = sizeof(*serverAddress);
    *server = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un tmp = {.sun_family = AF_UNIX};
    strcpy (tmp.sun_path, nomeServer);
    *serverAddress = tmp;
}

void connetti(TRENO trenoCorrente, int server, struct sockaddr *serverAddressPtr, int serverLen){
    int risultato;
    do { /* Loop until a connection is made with the server */
        risultato = connect (server, serverAddressPtr, serverLen);
        if (risultato == -1){
            aggiungiLog(trenoCorrente, "Connessione non riuscita con il server %s, riprovo in 3 secondi!", serverAddressPtr->sa_data);
	        sleep (3); /* Wait and then try again */
        }
    } while (risultato == -1);
    aggiungiLog(trenoCorrente, "Connessione stabilita con il server %s", serverAddressPtr->sa_data);
}

void inviaNumero(int fd, TRENO trenoCorrente){
    int numero = htonl(trenoCorrente.numTreno);
    while(send(fd, &numero, sizeof(numero), 0) < 0){
        aggiungiLog(trenoCorrente, "ERRORE: send non andata a buon fine, riprovo fra 2 secondi");
        sleep(2);
    }
}

void leggiElemento (int fd, char *str, TRENO trenoCorrente) {
    do {
        while(recv (fd, str, 1, 0) < 0){
            aggiungiLog(trenoCorrente, "ERRORE: receive non andata a buon fine, riprovo fra 2 secondi");
            sleep(2);
       }
    }while (*str++ != '\0');
}

void riceviItinerario (int fd, TRENO *trenoCorrente) {
    char str[200];
    int i = 0;
    char *prova[7];
    for(int i = 0; i<7; i++){
        leggiElemento(fd, str, *trenoCorrente);
        trenoCorrente->itinerario[i] = (char*) malloc(strlen(str)*sizeof(char));
        strcpy(trenoCorrente->itinerario[i], str);
    }
    aggiungiLog(*trenoCorrente,"Itinerario ricevuto con successo!");
}

int impegnaSegmentoETCS1(TRENO trenoCorrente, int numeroSegmento){
    char occupato[1];
    aggiungiLog(trenoCorrente, "Richiedo di accedere al file");
    if(numeroSegmento == 0) return 0;   //stazione, permesso sempre accordato
    sem_wait(semafori[numeroSegmento]);
    lseek(segmentiDescriptor[numeroSegmento], 0, SEEK_SET);
    read(segmentiDescriptor[numeroSegmento], &occupato, 1);
    if(strcmp(occupato,"1") == 0){
        sem_post(semafori[numeroSegmento]);
        return -1;
    }
    ftruncate(segmentiDescriptor[numeroSegmento], 0);
    lseek(segmentiDescriptor[numeroSegmento],0,SEEK_SET);
    write(segmentiDescriptor[numeroSegmento], "1", 1);
    sem_post(semafori[numeroSegmento]);
    return 0;
}

int impegnaSegmentoETCS2(TRENO trenoCorrente, int numeroSegmento, int RBC){
    char occupato[1];
    send(RBC, trenoCorrente.itinerario[trenoCorrente.posizioneProssima], sizeof(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale]), 0);
    recv(RBC, occupato, 1, 0);
    if(strcmp(occupato, "1") == 0){
        return -1;
    }
    aggiungiLog(trenoCorrente, "Permesso accordato da RBC!");
    return 0;
}

void liberaSegmento(TRENO trenoCorrente, int numeroSegmento, int RBC){
    ftruncate(segmentiDescriptor[numeroSegmento], 0);
    lseek(segmentiDescriptor[numeroSegmento], 0, SEEK_SET);
    write(segmentiDescriptor[numeroSegmento], "0", 1); //Il disimpegno è fatto senza preoccupazioni perchè si presuppone che nel peggiore dei casi porti ad una lettura in più
    send(RBC, trenoCorrente.itinerario[trenoCorrente.posizioneAttuale], sizeof(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale]),  0);
    aggiungiLog(trenoCorrente, "Segmento precedente liberato con successo!");
}

void eseguiStep(TRENO trenoCorrente, int numeroSegmentoProssimo, int RBC){
    int successo1;
    int successo2;
    do{
        if((successo1 = impegnaSegmentoETCS1(trenoCorrente, numeroSegmentoProssimo)) != 0){
            aggiungiLog(trenoCorrente, "Segmento occupato");
            if(ETCS == 1) sleep(2);
        }
        if(ETCS == 2){
            if((successo2 = impegnaSegmentoETCS2(trenoCorrente, numeroSegmentoProssimo, RBC)) != 0){
                aggiungiLog(trenoCorrente, "RBC rifiuta l'accesso");
            }
            if(successo1 != successo2){
                aggiungiLog(trenoCorrente, "Discordanza fra boe e RBC, treno bloccato in attesa di segnali!");
                pause();
            }else sleep(2);
        }
    }while(successo1 != 0);
    aggiungiLog(trenoCorrente, "Permesso accordato, impegno il prossimo segmento e libero il segmento attuale!");
}

int percorriItinerario(TRENO *trenoCorrente, int RBC){
    int numeroSegmentoProssimo;
    int numeroSegmentoAttuale;
    aggiungiLog(*trenoCorrente,"Inizio il mio persorso, esco dalla stazione!");
    for(trenoCorrente->posizioneAttuale = 0; trenoCorrente->posizioneAttuale < 6; trenoCorrente->posizioneAttuale++){
        aggiungiLog(*trenoCorrente,"[ATTUALE:%s],[NEXT:%s]",trenoCorrente->itinerario[trenoCorrente->posizioneAttuale],trenoCorrente->itinerario[trenoCorrente->posizioneAttuale+1]);
        numeroSegmentoProssimo = atoi(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale + 1] + 2);
        numeroSegmentoAttuale = atoi(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale] + 2);
        eseguiStep(*trenoCorrente, numeroSegmentoProssimo, RBC);
        liberaSegmento(*trenoCorrente, numeroSegmentoAttuale, RBC);
        sleep(2);
    }
    aggiungiLog(*trenoCorrente,"Concludo il mio percorso, sono in stazione!");
}

int main(int argc, char *argv[]) {
    int pid[7];
    signal(SIGUSR1, contaTerminazioniHandler);
    int registro, registroLen;
    int RBC, RBCLen;
    srand(time(NULL));
    TRENO trenoCorrente;
    ETCS = atoi(argv[1] + 4);
    struct sockaddr_un registroAddress; //Server address
    struct sockaddr* registroAddressPtr = (struct registroAddressPtr*) &registroAddress; //Server address pointer
    struct sockaddr_un RBCAddress;
    struct sockaddr* RBCAddressPtr = (struct RBCAddressPtr*) &RBCAddress;

    trenoCorrente.numTreno = 0;
    creaLog(&trenoCorrente);
    creaSegmenti();
    inizializzaSemafori();

    if(ETCS == 2){
        creaSocket(&RBC, &RBCAddress, &RBCLen, "RBC");
        connetti(trenoCorrente, RBC, RBCAddressPtr, RBCLen);
        printf("Sono qui\n");
        //recv(RBC, pid[6], 1, 0);
    }

    for(int i = 1; i<6; i++){
        if((pid[i] = fork()) < 0) exit(EXIT_FAILURE);
        else if(pid[i] == 0){
            trenoCorrente.numTreno = i;
            creaLog(&trenoCorrente);
            aggiungiLog(trenoCorrente,"Inizio del logging");
            creaSocket(&registro, &registroAddress, &registroLen, "RegistroTreni");
            connetti(trenoCorrente, registro, registroAddressPtr, registroLen);
            if(ETCS == 2){
                creaSocket(&RBC, &RBCAddress, &RBCLen, "RBC");
                connetti(trenoCorrente, RBC, RBCAddressPtr, RBCLen);
            }
            inviaNumero(registro, trenoCorrente);
            riceviItinerario(registro, &trenoCorrente);
            percorriItinerario(&trenoCorrente, RBC);
            preliminariTerminazione(&trenoCorrente);
            pause();
        }
        aggiungiLog(trenoCorrente, "Fork: %d creata con pid: %d", i, pid[i]);
    }

    /*Codice del padre*/
    aggiungiLog(trenoCorrente, "Padre treni è in attesa della terminazione dei figli:...");
    while(terminato < 5) pause();
    terminaProcessi(pid);
    aggiungiLog(trenoCorrente, "Tutti i treni sono arrivati a destinazione, termino");
    return 0;
}
