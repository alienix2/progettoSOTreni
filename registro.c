#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* For AFUNIX sockets */
#include <arpa/inet.h>
#define DEFAULT_PROTOCOL 0

//Registro di PROVA, ToDo quello vero

int receiveNumero(int fd, int *numeroTreno) {
    recv(fd, numeroTreno, sizeof(*numeroTreno), 0);
    *numeroTreno = ntohl(*numeroTreno);
    printf("Ho ricevuto %d, PID:%d\n", *numeroTreno, getpid());
}

int sendItinerario(int fd, char **itinerario[5][7], int numeroTreno){
    if(numeroTreno == 0){
        for(int k = 1; k<6; k++){
            for(int i = 0; i<7; i++){
                send(fd, itinerario[1][i], strlen(itinerario[1][i])+1, 0); //DA MODIFICARE 1 CON k
            }
        }
    }
    else{
        numeroTreno = 1; //DA ELIMINARE DOPO QUANDO SI METTONO LE MATRICI GIUSTE
        for(int i = 0; i<7; i++){
            send(fd, itinerario[numeroTreno][i], strlen(itinerario[numeroTreno][i])+1, 0);
        }
    }
    fflush(fd);
    return 0;
}

int main (int argc, char *argv[]) {
    int registro, clientFd, serverLen, clientLen;
    struct sockaddr_un registroAddress; /*Server address */
    struct sockaddr* serverSockAddrPtr; /*Ptr to server address*/
    struct sockaddr_un clientUNIXAddress; /*Client address */
    struct sockaddr* registroAddressPtr;/*Ptr to client address*/
    int numeroTreno;

    char *itinerario[6][7];
    itinerario[1][0] = "S0";
    itinerario[1][1] = "MA1";
    itinerario[1][2] = "MA2";
    itinerario[1][3] = "MA3";
    itinerario[1][4] = "MA4";
    itinerario[1][5] = "MA5";
    itinerario[1][6] = "S1";

    serverSockAddrPtr = (struct sockaddr*) &registroAddress;
    serverLen = sizeof (registroAddress);
    registroAddressPtr = (struct sockaddr*) &clientUNIXAddress;
    clientLen = sizeof (clientUNIXAddress);
  
    registro = socket (AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL);
    registroAddress.sun_family = AF_UNIX; /* Set domain type */
    strcpy (registroAddress.sun_path, "RegistroTreni"); /* Set name */
    unlink ("RegistroTreni"); /* Remove file if it already exists */
    bind (registro, serverSockAddrPtr, serverLen);/*Create file*/
    listen (registro, 6); /* Maximum pending connection length */
    while (1) {/* Loop forever */ /* Accept a client connection */
        clientFd = accept (registro, registroAddressPtr, &clientLen);
        if (fork () == 0) { /* Create child to send receipe */
            receiveNumero(clientFd, &numeroTreno);
            sendItinerario(clientFd, &itinerario, numeroTreno); //Da mettere al posto di 0 numeroTreno
        }else close(clientFd);/* Close the client descriptor */
    }
}
