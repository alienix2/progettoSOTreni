#include <stdio.h>
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

#include "./utilities/sharedFunctions.h"


char *itinerarioFolder = "./itinerario";    //Usato per poter cambiare a piacimento la directory per i file di testo MAX
char *logFolder = "./logs/treni"; //Usato per poter cambiare a piacimento la directory per i file di log
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
    if(ETCS == 2) kill(pid[7], SIGUSR2);
}

void riceviPid(int fd, int *pid){   //Funzione che riceve un numero e lo assegna correttamente a PID
    if(recv(fd, pid, sizeof(int), 0) < 0){
        aggiungiLog("ERRORE: Pid non ricevuto con successo, ritento in 2 secondi");
        sleep(2);
    }
    aggiungiLog("Pid ricevuto con successo, sono in grado di terminare RBC a fine programma");
    close(fd);
}

void creaSegmenti(){    //Funzione richiamata dal padre, inizializza 20 file di testo chiamati MAX con X in [1,17]
    char ma[20];
    for(int i = 1; i<17; i++){
        sprintf(ma, "%s/MA%d.txt", itinerarioFolder, i);
        remove(ma); //Rimuove il file se già presente, in modo da evitare file sporchi dopo un'esecuzione finita male
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
        sem_unlink(nomiSemafori[i]); //Rimuove il semaforo se già presente, in modo da evitare valori sporchi dopo un'esecuzione finita male
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

void connettiRBC(TRENO trenoCorrente, int *RBC){
    int RBCLen;
    struct sockaddr_un RBCAddress;  //Indirizzo di RBC
    struct sockaddr* RBCAddressPtr = (struct sockaddr*) &RBCAddress; //Puntatore all'inidirizzo di RBC
    accediSocket(RBC, &RBCAddress, &RBCLen, "ServerRBC");
    connetti(*RBC, RBCAddressPtr, RBCLen);
}


void inviaNumero(int fd, TRENO trenoCorrente){  //Invia un numero sul canale
    int numero = trenoCorrente.numTreno;
    while(send(fd, &numero, sizeof(numero), 0) < 0){
        aggiungiLog("ERRORE: send non andata a buon fine, riprovo fra 2 secondi");
        sleep(2);
    }
}

void riceviItinerario (int fd, TRENO *trenoCorrente) {  //Ricevo 7 stringhe e le metto correttamente all'interno dell'itinerario del treno
    char str[200];
    int i = 0;
    for(int i = 0; i<7; i++){
        leggiElemento(fd, str);
        trenoCorrente->itinerario[i] = (char*) malloc(strlen(str)*sizeof(char));
        strcpy(trenoCorrente->itinerario[i], str);
    }
    aggiungiLog("Itinerario ricevuto con successo!");
}

int impegnaSegmentoETCS1(TRENO trenoCorrente, int numeroSegmento){  //Funzione per la gestione di ETCS1
    char occupato[1];
    aggiungiLog("Richiedo di accedere al file");
    if(numeroSegmento == 0) return 0;   //stazione, permesso sempre accordato
    lseek(segmentiDescriptor[numeroSegmento], 0, SEEK_SET); //Mi posiziono ad inizio file
    read(segmentiDescriptor[numeroSegmento], &occupato, 1); //Leggo il contenuto
    if(strcmp(occupato,"1") == 0){  //Se è occupato ritorno -1
        return -1;
    }
    ftruncate(segmentiDescriptor[numeroSegmento], 0);   //Altrimenti eseguo correttamente una write di 0, eseguo una signal sul semaforo e ritorno 0
    lseek(segmentiDescriptor[numeroSegmento],0,SEEK_SET);
    write(segmentiDescriptor[numeroSegmento], "1", 1);
    return 0;
}

int impegnaSegmentoETCS2(TRENO trenoCorrente, int numeroSegmento, int RBC, int invio){ //Funzione per la gestione di ETCS2
    int occupato = 0;
    connettiRBC(trenoCorrente, &RBC);
    send(RBC, &invio, sizeof(int), 0);  //Invio prima il numero della richiesta effettuata
    send(RBC, &trenoCorrente.numTreno, sizeof(int), 0); //Dopodichè invio il numero del treno
    if(numeroSegmento != 0) send(RBC, &numeroSegmento, sizeof(int), 0);  //Infine invio il numero del segmento
    else {                                                                //Aggiungo 20 per identificare le stazioni
        invio = (atoi(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale + 1] + 1) + 20);
        send(RBC, &invio, sizeof(int), 0);
    }
    recv(RBC, &occupato, 1, 0); //Ricevo la risposta e chiudo il file descriptor
    close(RBC);
    if(occupato == 1){
        return -1;
    }
    aggiungiLog("Permesso accordato da RBC!");
    return 0;
}

void liberaSegmento(TRENO trenoCorrente, int numeroSegmento, int RBC, int invio){  //Funzione che libera il segmento corrente
    if(numeroSegmento != 0){    //Se non ho liberato una stazione
        ftruncate(segmentiDescriptor[numeroSegmento], 0);   //Tronco il file
        lseek(segmentiDescriptor[numeroSegmento], 0, SEEK_SET); //Vado alla posizione 0 e scrivo 0
        write(segmentiDescriptor[numeroSegmento], "0", 1); //Il disimpegno è fatto senza preoccupazioni perchè si presuppone che nel peggiore dei casi porti ad una lettura in più
    }
    if(ETCS == 2){  //Libero in RBC se ETCS == 2
        connettiRBC(trenoCorrente, &RBC);
        send(RBC, &invio, sizeof(int), 0);  //Invio il numero della richiesta
        send(RBC, &trenoCorrente.numTreno, sizeof(int), 0); //Invio il numero del treno
        if(numeroSegmento != 0) send(RBC, &numeroSegmento, sizeof(int), 0);  //invio il numero del segmento
        else {                                                               //Aggiungo 20 per identificare le stazioni
            invio = (atoi(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale] + 1) + 20);
            send(RBC, &invio, sizeof(int), 0);  
        }
        close(RBC);
    }
    aggiungiLog("Segmento precedente liberato con successo!");
}

void eseguiStep(TRENO trenoCorrente, int numeroSegmentoProssimo, int RBC){  //Funzione che gestisce i singoli step del treno
    int successo1 = 0;
    int successo2 = 0;
    do{
        if(numeroSegmentoProssimo != 0) sem_wait(semafori[numeroSegmentoProssimo]);
        if((successo1 = impegnaSegmentoETCS1(trenoCorrente, numeroSegmentoProssimo)) != 0){ //La funzione impegnaSegmentoETCS1 ritorna -1 se il segmento è occupato
            aggiungiLog("Segmento occupato");
        }
        if(ETCS == 2){  //Se sono in ETCS2 eseguo ulteriori controlli
            if((successo2 = impegnaSegmentoETCS2(trenoCorrente, numeroSegmentoProssimo, RBC, 1)) != 0){    //Come per impegnaSegmentoETCS1 ritorna -1 se il segmento è occupato
                aggiungiLog("RBC rifiuta l'accesso");
            }
            if(successo1 != successo2){ //Se discordanza fra impegnaSegmentoETCS1 e impegnaSegmentoETCS2, il treno va in pausa, in attesa di comandi dall'esterno
                aggiungiLog("Discordanza fra boe e RBC, treno bloccato in attesa di segnali!");
                pause();
            }
        }
        if(numeroSegmentoProssimo != 0) sem_post(semafori[numeroSegmentoProssimo]);
        sleep(2);
    }while(successo1 != 0); //Si controlla 1 solo fra successo1 e eventualmente successo2, perchè in ogni caso se discordassero il treno andrebbe in pausa
    aggiungiLog("Permesso accordato, impegno il prossimo segmento e libero il segmento attuale!");
}

int percorriItinerario(TRENO *trenoCorrente, int RBC){  //Funzione che scorre l'itinerario e chiama le funzioni per percorrerlo
    int numeroSegmentoProssimo; //Segmento a cui si dovrà accedere successivamente
    int numeroSegmentoAttuale;  //Segmento in cui ci troviamo
    aggiungiLog("Inizio il mio persorso, esco dalla stazione!");
    for(trenoCorrente->posizioneAttuale = 0; trenoCorrente->posizioneAttuale < 6 && strcmp(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale+1], "0") != 0; trenoCorrente->posizioneAttuale++){   //Tutti i treni eseguono 6 iterazioni
        aggiungiLog("[ATTUALE:%s],[NEXT:%s]",trenoCorrente->itinerario[trenoCorrente->posizioneAttuale],trenoCorrente->itinerario[trenoCorrente->posizioneAttuale+1]);
        numeroSegmentoProssimo = atoi(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale + 1] + 2);  //Restituisce il numero del segmento, con stazione restituisce 0
        numeroSegmentoAttuale = atoi(trenoCorrente->itinerario[trenoCorrente->posizioneAttuale] + 2);   //Restituisce il numero del segmento, con stazione restituisce 0
        eseguiStep(*trenoCorrente, numeroSegmentoProssimo, RBC);
        liberaSegmento(*trenoCorrente, numeroSegmentoAttuale, RBC, 0);
        sleep(2);
    }
    aggiungiLog("Concludo il mio percorso, sono in stazione!");
}

int main(int argc, char *argv[]) {
    int pid[7];
    signal(SIGUSR1, contaTerminazioniHandler);
    int registro, registroLen;
    int RBC, RBCLen;
    TRENO trenoCorrente;
    ETCS = atoi(argv[1] + 4); //Interpreto la X nella scritta "ETCSX"
    struct sockaddr_un registroAddress; //Indirizzo del registro
    struct sockaddr* registroAddressPtr = (struct sockaddr*) &registroAddress; //Puntatore all'indirizzo del registro
    struct sockaddr_un RBCAddress;  //Indirizzo di RBC
    struct sockaddr* RBCAddressPtr = (struct sockaddr*) &RBCAddress; //Puntatore all'inidirizzo di RBC

    trenoCorrente.numTreno = 0;
    creaLog(&trenoCorrente);
    logFile = trenoCorrente.log;
    creaSegmenti();
    inizializzaSemafori();
    if(ETCS == 2){
        accediSocket(&RBC, &RBCAddress, &RBCLen, "ServerRBC");
        connetti(RBC, RBCAddressPtr, RBCLen);
        riceviPid(RBC, &pid[7]);
    }
    for(int i = 1; i<6; i++){   //Uso un ciclo da 1 a 6 per comodità
        if((pid[i] = fork()) < 0) exit(EXIT_FAILURE);
        else if(pid[i] == 0){ //Codice dei figli
            trenoCorrente.numTreno = i;
            creaLog(&trenoCorrente);
            logFile = trenoCorrente.log;
            aggiungiLog("Inizio del logging, mi connetto a RegistroTreni");
            accediSocket(&registro, &registroAddress, &registroLen, "RegistroTreni");
            connetti(registro, registroAddressPtr, registroLen);
            inviaNumero(registro, trenoCorrente);
            riceviItinerario(registro, &trenoCorrente);
            if(strcmp(trenoCorrente.itinerario[trenoCorrente.posizioneAttuale], "0") != 0){ //Se il primo segmento contiene "0" il treno non è necessario
                percorriItinerario(&trenoCorrente, RBC);
            }
            else{
               aggiungiLog("Treno non necessario, non in partenza dalla stazione!");
            }
            preliminariTerminazione(&trenoCorrente);
            pause(); //Si mette in attesa di segnali
        }
        aggiungiLog("Fork: %d creata con pid: %d", i, pid[i]); //eseguito dal padre
    }

    //Codice del padre
    aggiungiLog("Padre treni è in attesa della terminazione dei figli:...");
    while(terminato < 5) pause(); //In attesa di 5 segnali
    terminaProcessi(pid);
    aggiungiLog("Tutti i treni sono arrivati a destinazione, termino");
    fclose(trenoCorrente.log);
    return 0;
}
