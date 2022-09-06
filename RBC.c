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
#include <sys/un.h> /* For AFUNIX sockets */
#define DEFAULT_PROTOCOL 0

char *logFolder = "./logs/RBC";
FILE *logFile;

void terminaProcessoHandler(){
    aggiungiLog("Sono stato terminato con successo da padreTreni!");
    fclose(logFile);
    exit(EXIT_SUCCESS);
}

char *getTime(char *data){  //Funzione di servizio per ottenere la data nel formato desiderato
    time_t tempo;
    struct tm *info;
    time(&tempo);
    info = localtime(&tempo);
    strftime(data,50,",\t%x - %X\n", info);
    return data;
}

void aggiungiLog(const char *fmt, ...){    //Aggiunge la stringa con parametri al file di log fi RBC
    va_list args;
    char messaggioLog[100];
    char data[50];
    va_start(args, fmt);
    vsnprintf(messaggioLog, 100, fmt, args);
    va_end(args);
    strcat(messaggioLog, getTime(data));
    fputs(messaggioLog, logFile);
    fflush(logFile);
}

void creaLog(){ //Funzione che crea un log partendo dal numeroTreno del treno che la invoca
    char nomeLog[20];
    sprintf(nomeLog, "%s/RBC.log",logFolder);
    logFile = fopen(nomeLog, "w");
}

//RBC di PROVA, ToDo quello vero
void leggiElemento (int fd, char *str) {   //Funzione generica per ricevere un numero non definito di caratteri
    do {
        while(recv (fd, str, 1, 0) < 0){
            sleep(2);
       }
    }while (*str++ != '\0');
}

void accediSocket(int *server, struct sockaddr_un *serverAddress, int *serverLen, char* nomeServer){  //Funzione che crea una socket partendo dai dati che riceve come parametri
    *serverLen = sizeof(*serverAddress);
    *server = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un tmp = {.sun_family = AF_UNIX};
    strcpy (tmp.sun_path, nomeServer);
    *serverAddress = tmp;
}

void connetti(int server, struct sockaddr *serverAddressPtr, int serverLen){   //Funzione che crea una connessione coi dati che riceve come parametri
    int risultato;
    do { /* Loop until a connection is made with the server */
        risultato = connect (server, serverAddressPtr, serverLen);
        if (risultato == -1){
            aggiungiLog("Connessione non riuscita con il server %s, riprovo in 3 secondi!", serverAddressPtr->sa_data);
	        sleep (3); /* Wait and then try again */
        }
    } while (risultato == -1);
    aggiungiLog("Connessione stabilita con il server %s", serverAddressPtr->sa_data);
}


int receiveNumero(int fd) {
    int numero;
    recv(fd, &numero, sizeof(numero), 0);
    numero = ntohl(numero);
    printf("Ho ricevuto %d, PID:%d\n", numero, getpid());
}

void riceviItinerari (int fd, char *itinerari[6][7]) {  //Ricevo 7 stringhe e le metto correttamente all'interno dell'itinerario del treno
    char str[200];
    int i = 0;
    char *prova[7];
    for(int k = 1; k<6; k++){
        for(int i = 0; i<7; i++){
            leggiElemento(fd, str);
            itinerari[k][i] = (char*) malloc(strlen(str)*sizeof(char));
            strcpy(itinerari[k][i], str);
        }
    }
}

void inviaNumero(int fd){  //Invia un numero sul canale
    int numero = htonl(0);
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
    invio = htonl(getpid());
    while(send(clientFd, &invio, sizeof(invio), 0) < 0){
        sleep(2);   //In caso di fallimento
    }
    aggiungiLog("PID inviato con successo a padreTreni per la terminazione finale");
    close(clientFd);
}

void gestisciAccesso(int clientFd, int segmenti[17], int stazioni[9]){
    int numeroTreno;
    int numeroSegmento;
    int invio;
    recv(clientFd, &numeroTreno, sizeof(int), 0);   //Ricevo il numero del treno per composizione log
    printf("Ho ricevuto il numero del treno!: %d\n", numeroTreno);
    recv(clientFd, &numeroSegmento, sizeof(int), 0);    //Ricevo la posizione dove intende accedere
    perror("recv");
    printf("Ho ricevuto il tipo di richiesta!\n");
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
    if(numeroSegmento > 20) stazioni[numeroSegmento - 20]--;
    else segmenti[numeroSegmento]--;
    aggiungiLog("Il treno numero:%d ha lasciato il segmento:MA%d", numeroTreno, numeroSegmento);
}

void gestisciRichiesta(int RBC, struct sockaddr *RBCAddressPtr, int clientLen, int segmenti[17], int stazioni[8]){
    int richiesta;
    int clientFd = accept (RBC, RBCAddressPtr, &clientLen); //Accetto una connessione
    printf("Ho accettato una connessione!\n");
    recv(clientFd, &richiesta, sizeof(int), 0); //Ricevo il numero della richiesta, 1 accesso, 0 abbandono
    printf("Richiesta ricevuta: %d\n", richiesta);
    if(richiesta == 1) gestisciAccesso(clientFd, segmenti, stazioni);   //1 accesso
    else gestisciAbbandono(clientFd, segmenti, stazioni);   //0 abbandono
    printf("Ho gestito la richiesta!\n");
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
    struct sockaddr* RBCAddressPtr = (struct registroAddressPtr*) &RBCAddress; //Puntatore all'indirizzo del registro

    struct sockaddr_un registroAddress; //Indirizzo del registro
    struct sockaddr* registroAddressPtr = (struct registroAddressPtr*) &registroAddress; //Puntatore all'indirizzo del registro

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