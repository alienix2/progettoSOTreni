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

char *itinerarioFolder = "./itinerario";    //Usato per poter cambiare a piacimento la directory per i file di testo MAX
char *logFolder = "./logs"; //Usato per poter cambiare a piacimento la directory per i file di log
//Le seguenti variabili sono inserite qui per evitare di doverle passare svariate volte fra le funzioni
int segmentiDescriptor[17];
int ETCS;
int terminato = 0;
sem_t *semafori[17];
char *nomiSemafori[17];

typedef struct{ //Struct che decrive lo stato di un treno
    int numTreno;
    int posizioneAttuale;
    int posizioneProssima;
    char *itinerario[7];
    FILE *log;
}TRENO;

char *getTime(char *data){  //Funzione di servizio per ottenere la data nel formato desiderato
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

void preliminariTerminazione(TRENO *trenoCorrente){ //Funzione richiamata dai figli che libera la memoria da itinerario e chiude i file di log
    srand(getpid());    //Generatore di numeri random con seed pid del processo che lo richiama
    for(int i = 0; i<7; i++){
        free(trenoCorrente->itinerario[i]);
    }
    fclose(trenoCorrente->log);
    usleep((rand()%1000000));   //Aspetto un tempo random prima di mandare la SIGUSR1 al padre
    kill(getppid(), SIGUSR1);
}

void terminaProcessi(int pid[7]){   //Funzione richiamata dal padre, chiude tutti i fileDescriptor, e procede a killare i figli
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

void aggiungiLog(TRENO trenoCorrente, const char *fmt, ...){    //Aggiunge la stringa con parametri al giusto file di log
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

void riceviPid(int fd, int *pid){
    if(recv(fd, pid, sizeof(int), 0) < 0){
        perror("recv");
    }
    pid = htonl(pid);
    close(fd);
}

void creaSegmenti(){    //Funzione richiamata dal padre, inizializza 20 file di testo chiamati MAX con X in [1,17]
    char ma[20];
    for(int i = 1; i<17; i++){
        sprintf(ma, "%s/MA%d.txt", itinerarioFolder, i);
        segmentiDescriptor[i] = open(ma, O_CREAT|O_RDWR);
        fchmod(segmentiDescriptor[i], S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
        if(write(segmentiDescriptor[i], "0", 1) < 0) exit(EXIT_FAILURE);
    }
}

void inizializzaSemafori(){ //Funzione richiamata dal padre, inizializza 17 semafori chiamati semX con X in [1,17]
    char nome[10];
    for(int i = 1; i<17; i++){
        nomiSemafori[i] = (char*) malloc(strlen(nome));
        sprintf(nomiSemafori[i], "sem%d", i);
        semafori[i] = sem_open(nomiSemafori[i], O_CREAT, 0777, 1);
    }
}

void creaLog(TRENO *trenoCorrente){ //Funzione che crea un log partendo dal numeroTreno del treno che la invoca
    char nomeLog[20];
    sprintf(nomeLog, "%s/T%d.log",logFolder, trenoCorrente->numTreno);
    if((trenoCorrente->log = fopen(nomeLog, "w")) < 0){
        kill(getppid(), SIGUSR1);
        kill(getpid(), SIGKILL);
        exit(EXIT_FAILURE);
    }
}

void creaSocket(int *server, struct sockaddr_un *serverAddress, int *serverLen, char* nomeServer){  //Funzione che crea una socket partendo dai dati che riceve come parametri
    *serverLen = sizeof(*serverAddress);
    *server = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un tmp = {.sun_family = AF_UNIX};
    strcpy (tmp.sun_path, nomeServer);
    *serverAddress = tmp;
}

void connetti(TRENO trenoCorrente, int server, struct sockaddr *serverAddressPtr, int serverLen){   //Funzione che crea una connessione coi dati che riceve come parametri
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

void inviaNumero(int fd, TRENO trenoCorrente){  //Invia un numero sul canale
    int numero = htonl(trenoCorrente.numTreno);
    while(send(fd, &numero, sizeof(numero), 0) < 0){
        aggiungiLog(trenoCorrente, "ERRORE: send non andata a buon fine, riprovo fra 2 secondi");
        sleep(2);
    }
}

void leggiElemento (int fd, char *str, TRENO trenoCorrente) {   //Funzione generica per ricevere un numero non definito di caratteri
    do {
        while(recv (fd, str, 1, 0) < 0){
            aggiungiLog(trenoCorrente, "ERRORE: receive non andata a buon fine, riprovo fra 2 secondi");
            sleep(2);
       }
    }while (*str++ != '\0');
}

void riceviItinerario (int fd, TRENO *trenoCorrente) {  //Ricevo 7 stringhe e le metto correttamente all'interno dell'itinerario del treno
    char str[200];
    int i = 0;
    for(int i = 0; i<7; i++){
        leggiElemento(fd, str, *trenoCorrente);
        trenoCorrente->itinerario[i] = (char*) malloc(strlen(str)*sizeof(char));
        strcpy(trenoCorrente->itinerario[i], str);
    }
    aggiungiLog(*trenoCorrente,"Itinerario ricevuto con successo!");
}

int impegnaSegmentoETCS1(TRENO trenoCorrente, int numeroSegmento){  //Funzione per la gestione di ETCS1
    char occupato[1];
    aggiungiLog(trenoCorrente, "Richiedo di accedere al file");
    if(numeroSegmento == 0) return 0;   //stazione, permesso sempre accordato
    sem_wait(semafori[numeroSegmento]); //Invio una wait sul semaforo corretto
    lseek(segmentiDescriptor[numeroSegmento], 0, SEEK_SET); //Mi posiziono ad inizio file
    read(segmentiDescriptor[numeroSegmento], &occupato, 1); //Leggo il contenuto
    if(strcmp(occupato,"1") == 0){  //Se è occupato ritorno -1
        sem_post(semafori[numeroSegmento]);
        return -1;
    }
    ftruncate(segmentiDescriptor[numeroSegmento], 0);   //Altrimenti eseguo correttamente una write di 0, eseguo una signal sul semaforo e ritorno 0
    lseek(segmentiDescriptor[numeroSegmento],0,SEEK_SET);
    write(segmentiDescriptor[numeroSegmento], "1", 1);
    sem_post(semafori[numeroSegmento]);
    return 0;
}

int impegnaSegmentoETCS2(TRENO trenoCorrente, int numeroSegmento, int RBC){ //Funzione per la gestione di ETCS2 *ToDo in parte?*
    char occupato[1];
    printf("Sono qui! in impegna\n");
    send(RBC, &trenoCorrente.itinerario[trenoCorrente.posizioneProssima], sizeof(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale]), 0);
    printf("Sono dopo la send\n");
    recv(RBC, occupato, 1, 0);
    if(strcmp(occupato, "1") == 0){
        return -1;
    }
    aggiungiLog(trenoCorrente, "Permesso accordato da RBC!");
    return 0;
}

void liberaSegmento(TRENO trenoCorrente, int numeroSegmento, int RBC){  //Funzione che libera il segmento corrente
    ftruncate(segmentiDescriptor[numeroSegmento], 0);   //Tronco il file
    lseek(segmentiDescriptor[numeroSegmento], 0, SEEK_SET); //Vado alla posizione 0 e scrivo 0
    write(segmentiDescriptor[numeroSegmento], "0", 1); //Il disimpegno è fatto senza preoccupazioni perchè si presuppone che nel peggiore dei casi porti ad una lettura in più
    if(ETCS == 2) send(RBC, &trenoCorrente.itinerario[trenoCorrente.posizioneAttuale], sizeof(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale]),  0); //Libero anche in RBC
    aggiungiLog(trenoCorrente, "Segmento precedente liberato con successo!");
}

void eseguiStep(TRENO trenoCorrente, int numeroSegmentoProssimo, int RBC){  //Funzione che gestisce i singoli step del treno
    int successo1;
    int successo2;
    do{
        if((successo1 = impegnaSegmentoETCS1(trenoCorrente, numeroSegmentoProssimo)) != 0){ //La funzione impegnaSegmentoETCS1 ritorna -1 se il segmento è occupato
            aggiungiLog(trenoCorrente, "Segmento occupato");
            if(ETCS == 1) sleep(2);
        }
        if(ETCS == 2){  //Se sono in ETCS2 eseguo ulteriori controlli
            if((successo2 = impegnaSegmentoETCS2(trenoCorrente, numeroSegmentoProssimo, RBC)) != 0){    //Come per impegnaSegmentoETCS1 ritorna -1 se il segmento è occupato
                aggiungiLog(trenoCorrente, "RBC rifiuta l'accesso");
                sleep(2);
            }
            if(successo1 != successo2){ //Se discordanza fra impegnaSegmentoETCS1 e impegnaSegmentoETCS2, il treno va in pausa, in attesa di comandi dall'esterno
                aggiungiLog(trenoCorrente, "Discordanza fra boe e RBC, treno bloccato in attesa di segnali!");
                pause();
            }
        }
    }while(successo1 != 0); //Si controlla 1 solo fra successo1 e eventualmente successo2, perchè in ogni caso se discordassero il treno andrebbe in pausa
    aggiungiLog(trenoCorrente, "Permesso accordato, impegno il prossimo segmento e libero il segmento attuale!");
}

int percorriItinerario(TRENO *trenoCorrente, int RBC){  //Funzione che scorre l'itinerario e chiama le funzioni per percorrerlo
    int numeroSegmentoProssimo; //Segmento a cui si dovrà accedere successivamente
    int numeroSegmentoAttuale;  //Segmento in cui ci troviamo
    aggiungiLog(*trenoCorrente,"Inizio il mio persorso, esco dalla stazione!");
    for(trenoCorrente->posizioneAttuale = 0; trenoCorrente->posizioneAttuale < 6 && strcmp(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale+1], "0") != 0; trenoCorrente->posizioneAttuale++){   //Tutti i treni eseguono 6 iterazioni
        aggiungiLog(*trenoCorrente,"[ATTUALE:%s],[NEXT:%s]",trenoCorrente->itinerario[trenoCorrente->posizioneAttuale],trenoCorrente->itinerario[trenoCorrente->posizioneAttuale+1]);
        numeroSegmentoProssimo = atoi(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale + 1] + 2);  //Restituisce il numero del segmento, con stazione restituisce 0
        numeroSegmentoAttuale = atoi(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale] + 2);   //Restituisce il numero del segmento, con stazione restituisce 0
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
    int RBC, RBCLen, RBCPid;
    TRENO trenoCorrente;
    ETCS = atoi(argv[1] + 4); //Interpreto la X nella scritta "ETCSX"
    struct sockaddr_un registroAddress; //Indirizzo del registro
    struct sockaddr* registroAddressPtr = (struct registroAddressPtr*) &registroAddress; //Puntatore all'indirizzo del registro
    struct sockaddr_un RBCAddress;  //Indirizzo di RBC
    struct sockaddr* RBCAddressPtr = (struct RBCAddressPtr*) &RBCAddress; //Puntatore all'inidirizzo di RBC

    trenoCorrente.numTreno = 0;
    creaLog(&trenoCorrente);
    creaSegmenti();
    inizializzaSemafori();
    if(ETCS == 2){
        creaSocket(&RBC, &RBCAddress, &RBCLen, "ServerRBC");
        connetti(trenoCorrente, RBC, RBCAddressPtr, RBCLen);
        riceviPid(RBC, &RBCPid);
        printf("%d\n", RBCPid);
    }
    for(int i = 1; i<6; i++){
        if((pid[i] = fork()) < 0) exit(EXIT_FAILURE);
        else if(pid[i] == 0){ //Codice dei figli
            trenoCorrente.numTreno = i;
            creaLog(&trenoCorrente);
            aggiungiLog(trenoCorrente,"Inizio del logging, mi connetto a RegistroTreni");
            creaSocket(&registro, &registroAddress, &registroLen, "RegistroTreni");
            connetti(trenoCorrente, registro, registroAddressPtr, registroLen);
            if(ETCS == 2){ //Da rivedere
                aggiungiLog(trenoCorrente,"Mi connetto a RBC");
                creaSocket(&RBC, &RBCAddress, &RBCLen, "RBC");
                connetti(trenoCorrente, RBC, RBCAddressPtr, RBCLen);
            }
            inviaNumero(registro, trenoCorrente);
            riceviItinerario(registro, &trenoCorrente);
            if(strcmp(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale], "0") != 0){
                percorriItinerario(&trenoCorrente, RBC);
            }
            else{
               aggiungiLog(trenoCorrente,"Treno non necessario, non in partenza dalla stazione!");
            }
            preliminariTerminazione(&trenoCorrente);
            pause(); //Si mette in attesa di segnali
        }
        aggiungiLog(trenoCorrente, "Fork: %d creata con pid: %d", i, pid[i]); //eseguito dal padre
    }

    //Codice del padre
    aggiungiLog(trenoCorrente, "Padre treni è in attesa della terminazione dei figli:...");
    while(terminato < 5) pause(); //In attesa di 5 segnali
    terminaProcessi(pid);
    aggiungiLog(trenoCorrente, "Tutti i treni sono arrivati a destinazione, termino");
    return 0;
}
