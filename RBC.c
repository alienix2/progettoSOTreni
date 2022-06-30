#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/un.h> /* For AFUNIX sockets */
#define DEFAULT_PROTOCOL 0

//RBC di PROVA, ToDo quello vero
void leggiElemento (int fd, char *str) {   //Funzione generica per ricevere un numero non definito di caratteri
    do {
        while(recv (fd, str, 1, 0) < 0){
            sleep(2);
       }
    }while (*str++ != '\0');
}

void creaSocket(int *server, struct sockaddr_un *serverAddress, int *serverLen, char* nomeServer){  //Funzione che crea una socket partendo dai dati che riceve come parametri
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
	        sleep (3); /* Wait and then try again */
        }
    } while (risultato == -1);
}


int receiveNumero(int fd) {
    int numero;
    recv(fd, &numero, sizeof(numero), 0);
    numero = ntohl(numero);
    printf("Ho ricevuto %d, PID:%d\n", numero, getpid());
}

void riceviItinerari (int fd, char **itinerario[6][7]) {  //Ricevo 7 stringhe e le metto correttamente all'interno dell'itinerario del treno
    char str[200];
    int i = 0;
    char *prova[7];
    for(int k = 1; k<6; k++){
        for(int i = 0; i<7; i++){
            leggiElemento(fd, str);
            itinerario[k][i] = (char*) malloc(strlen(str)*sizeof(char));
            strcpy(itinerario[k][i], str);
        }
    }
}

void inviaNumero(int fd){  //Invia un numero sul canale
    int numero = htonl(0);
    while(send(fd, &numero, sizeof(numero), 0) < 0){
        sleep(2);
    }
}

void inviaPid(int RBC, struct sockaddr *RBCAddressPtr, int clientLen){
    int invio;
    int clientFd = accept (RBC, RBCAddressPtr, &clientLen);
    invio = htonl(getpid());
    while(send(clientFd, &invio, sizeof(invio), 0) < 0){
        sleep(2);
    }
}

int main (void) {
    int registro, registroLen;
    int RBC, RBClen;
    int clientFd, serverLen, clientLen;
    char *itinerari[6][7];
    char *segmenti[17];
    char *stazioni[8];

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
    
    creaSocket(&registro, &registroAddress, &registroLen, "RegistroTreni");
    connetti(registro, registroAddressPtr, registroLen);
    inviaNumero(registro);
    riceviItinerari(registro, &itinerari);

    RBC = socket (AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL);
    RBCAddress.sun_family = AF_UNIX; /* Set domain type */
    strcpy (RBCAddress.sun_path, "ServerRBC"); /* Set name */
    unlink ("ServerRBC"); /* Remove file if it already exists */
    bind (RBC, serverSockAddrPtr, serverLen);/*Create file*/
    listen (RBC, 6); /* Maximum pending connection length */
    inviaPid(RBC, RBCAddressPtr, clientLen);

    //creaConnessione();
    return 0;
}