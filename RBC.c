#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* For AFUNIX sockets */
#define DEFAULT_PROTOCOL 0

#include "./utilities/sharedLibraries.h"

char *logFolder = "./logs/RBC";

void creaLog(){ //Funzione che crea un log partendo dal numeroTreno del treno che la invoca
    char nomeLog[20];
    sprintf(nomeLog, "%s/RBC.log",logFolder);
    logFile = fopen(nomeLog, "w");
}

void terminaProcessoHandler(){
    aggiungiLog("Sono stato terminato con successo da padreTreni!");
    fclose(logFile);
    exit(EXIT_SUCCESS);
}

int receiveNumero(int fd) {
    int numero;
    recv(fd, &numero, sizeof(numero), 0);
}

void riceviItinerari (int fd, char *itinerari[6][7]) {  //Ricevo 7 stringhe e le metto correttamente all'interno dell'itinerario del treno
    char str[200];
    for(int k = 1; k<6; k++){
        for(int i = 0; i<7; i++){
            leggiElemento(fd, str);
            itinerari[k][i] = (char*) malloc(strlen(str)*sizeof(char));
            strcpy(itinerari[k][i], str);
        }
    }
}

void inviaNumero(int fd){  //Invia un numero sul canale
    int numero = 0;
    while(send(fd, &numero, sizeof(numero), 0) < 0){
        sleep(2);
    }
    aggiungiLog("Numero inviato con successo a registro!");
}

void impostaStazioni(char *itinerari[6][7], int stazioni[9]){
    int numeroStazione;
    for(int k = 1; k<6; k++){
        if((numeroStazione = atoi(itinerari[k][0] + 1)) != 0)   //Per evitare di incrementare la stazione 0 nel caso di mapppa1
        stazioni[numeroStazione]++;
    }
}

void inviaPid(int RBC, struct sockaddr *RBCAddressPtr, int clientLen){
    int invio;
    int clientFd = accept (RBC, RBCAddressPtr, &clientLen);
    invio = getpid();
    while(send(clientFd, &invio, sizeof(invio), 0) < 0){
        aggiungiLog("ERRORE: PID non inviato con successo a padreTreni, ritento in 3 secondi");
        sleep(3);   //In caso di fallimento
    }
    aggiungiLog("PID inviato con successo a padreTreni per la terminazione finale");
    close(clientFd);
}

void gestisciAccesso(int clientFd, int segmenti[17], int stazioni[9]){
    int numeroTreno;
    int numeroSegmento;
    int invio;
    recv(clientFd, &numeroTreno, sizeof(int), 0);   //Ricevo il numero del treno per composizione log
    recv(clientFd, &numeroSegmento, sizeof(int), 0);    //Ricevo la posizione dove intende accedere
    if(numeroSegmento > 20) stazioni[numeroSegmento - 20]++;
    else if(segmenti[numeroSegmento] == 0){
        invio = 0;
        send(clientFd, &invio, sizeof(int), 0);   //0 risposta positiva
        segmenti[numeroSegmento]++;
        aggiungiLog("Il treno numero:%d ha richiesto di accedere al segmento:MA%d, permesso accordato!", numeroTreno, numeroSegmento);
    }
    else {
        invio = 1;
        send(clientFd, &invio, sizeof(int), 0); //1 risposta negativa
        aggiungiLog("Il treno numero:%d ha richiesto di accedere al segmento:MA%d, permesso rifiutato!", numeroTreno, numeroSegmento);
    }
}

void gestisciAbbandono(int clientFd, int segmenti[17], int stazioni[8]){
    int numeroTreno;
    int numeroSegmento;
    recv(clientFd, &numeroTreno, sizeof(int), 0);   //Ricevo il numero del treno per composizione log
    recv(clientFd, &numeroSegmento, sizeof(int), 0);    //Ricevo la posizione che lascia
    if(numeroSegmento > 20) {
        aggiungiLog("Il treno numero:%d ha lasciato la stazione:S%d", numeroTreno, (numeroSegmento-20));
        stazioni[numeroSegmento - 20]--;
    }
    else {
        aggiungiLog("Il treno numero:%d ha lasciato il segmento:MA%d", numeroTreno, numeroSegmento);
        segmenti[numeroSegmento]--;
    }
    
}

void gestisciRichiesta(int RBC, struct sockaddr *RBCAddressPtr, int clientLen, int segmenti[17], int stazioni[8]){
    int richiesta;
    int clientFd = accept (RBC, RBCAddressPtr, &clientLen); //Accetto una connessione
    recv(clientFd, &richiesta, sizeof(int), 0); //Ricevo il numero della richiesta, 1 accesso, 0 abbandono
    if(richiesta == 1) gestisciAccesso(clientFd, segmenti, stazioni);   //1 accesso
    else gestisciAbbandono(clientFd, segmenti, stazioni);   //0 abbandono
    close(clientFd);    //Chiudo il file descriptor del client
}

int main (void) {
    signal(SIGUSR2, terminaProcessoHandler);
    int registro, registroLen;
    int RBC, RBClen;
    int clientFd, serverLen, clientLen;
    char *itinerari[6][7];
    int occupazioneSegmenti[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int stazioni[9] = {0,0,0,0,0,0,0,0,0};

    struct sockaddr* serverSockAddrPtr; /*Ptr to server address*/
    struct sockaddr_un clientUNIXAddress; /*Client address */
    struct sockaddr_un RBCAddress; //Indirizzo del registro
    struct sockaddr* RBCAddressPtr = (struct sockaddr*) &RBCAddress; //Puntatore all'indirizzo del registro

    struct sockaddr_un registroAddress; //Indirizzo del registro
    struct sockaddr* registroAddressPtr = (struct sockaddr*) &registroAddress; //Puntatore all'indirizzo del registro

    serverSockAddrPtr = (struct sockaddr*) &RBCAddress;
    serverLen = sizeof (RBCAddress);
    RBCAddressPtr = (struct sockaddr*) &clientUNIXAddress;
    clientLen = sizeof (clientUNIXAddress);
    
    creaLog();

    accediSocket(&registro, &registroAddress, &registroLen, "RegistroTreni");
    connetti(registro, registroAddressPtr, registroLen);
    inviaNumero(registro);
    riceviItinerari(registro, itinerari);
    impostaStazioni(itinerari, stazioni);

    RBC = socket (AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL);
    RBCAddress.sun_family = AF_UNIX; // Set domain type

    strcpy (RBCAddress.sun_path, "ServerRBC"); // Set name 
    unlink ("ServerRBC"); //Remove file if it already exists

    bind (RBC, serverSockAddrPtr, serverLen);//Create file
    listen (RBC, 0); //Maximum pending connection length

    inviaPid(RBC, RBCAddressPtr, clientLen);

    while(1){
        gestisciRichiesta(RBC, RBCAddressPtr, clientLen, occupazioneSegmenti, stazioni);
    }
    //creaConnessione();*/
    return 0;
}