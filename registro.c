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
void riempiUno(char *itinerario[6][7]){
    itinerario[1][0] = "S1";
    itinerario[1][1] = "MA1";
    itinerario[1][2] = "MA2";
    itinerario[1][3] = "MA3";
    itinerario[1][4] = "MA8";
    itinerario[1][5] = "S6";
    itinerario[1][6] = "0";

    itinerario[2][0] = "S2";
    itinerario[2][1] = "MA5";
    itinerario[2][2] = "MA6";
    itinerario[2][3] = "MA7";
    itinerario[2][4] = "MA3";
    itinerario[2][5] = "MA8";
    itinerario[2][6] = "S6";

    itinerario[3][0] = "S7";
    itinerario[3][1] = "MA13";
    itinerario[3][2] = "MA12";
    itinerario[3][3] = "MA11";
    itinerario[3][4] = "MA10";
    itinerario[3][5] = "MA9";
    itinerario[3][6] = "S3";

    itinerario[4][0] = "S4";
    itinerario[4][1] = "MA14";
    itinerario[4][2] = "MA15";
    itinerario[4][3] = "MA16";
    itinerario[4][4] = "MA12";
    itinerario[4][5] = "S8";
    itinerario[4][6] = "0";

    itinerario[5][0] = "0";
    itinerario[5][1] = "0";
    itinerario[5][2] = "0";
    itinerario[5][3] = "0";
    itinerario[5][4] = "0";
    itinerario[5][5] = "0";
    itinerario[5][6] = "0";
}

void riempiDue(char *itinerario[6][7]){    //ToDo
    itinerario[1][0] = "S2";
    itinerario[1][1] = "MA5";
    itinerario[1][2] = "MA6";
    itinerario[1][3] = "MA7";
    itinerario[1][4] = "MA3";
    itinerario[1][5] = "MA8";
    itinerario[1][6] = "S6";

    itinerario[2][0] = "S3";
    itinerario[2][1] = "MA9";
    itinerario[2][2] = "MA10";
    itinerario[2][3] = "MA11";
    itinerario[2][4] = "MA12";
    itinerario[2][5] = "S8";
    itinerario[2][6] = "0";

    itinerario[3][0] = "S4";
    itinerario[3][1] = "MA14";
    itinerario[3][2] = "MA15";
    itinerario[3][3] = "MA16";
    itinerario[3][4] = "MA12";
    itinerario[3][5] = "S8";
    itinerario[3][6] = "0";

    itinerario[4][0] = "S6";
    itinerario[4][1] = "MA8";
    itinerario[4][2] = "MA3";
    itinerario[4][3] = "MA2";
    itinerario[4][4] = "MA1";
    itinerario[4][5] = "S1";
    itinerario[4][6] = "0";

    itinerario[5][0] = "S5";
    itinerario[5][1] = "MA4";
    itinerario[5][2] = "MA3";
    itinerario[5][3] = "MA2";
    itinerario[5][4] = "MA1";
    itinerario[5][5] = "S1";
    itinerario[5][6] = "0";
}

int receiveNumero(int fd, int *numeroTreno) {
    recv(fd, numeroTreno, sizeof(numeroTreno), 0);
}

int sendItinerario(int fd, char *itinerario[6][7], int numeroTreno){
    if(numeroTreno == 0){
        for(int k = 1; k<6; k++){
            for(int i = 0; i<7; i++){
                send(fd, itinerario[k][i], strlen(itinerario[k][i])+1, 0);
            }
        }
    }
    else{
        for(int i = 0; i<7; i++){
            send(fd, itinerario[numeroTreno][i], strlen(itinerario[numeroTreno][i])+1, 0);
        }
    }
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
    if(strcmp(argv[1], "MAPPA1") == 0){ //Selezione della mappa basata su parametro passato dal launcher
        riempiUno(itinerario);
    }
    else riempiDue(itinerario);

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
            sendItinerario(clientFd, itinerario, numeroTreno);
            exit(EXIT_SUCCESS);
        }
        else{
            signal(SIGCHLD, SIG_IGN); 
            close(clientFd);/* Close the client descriptor */
        }
    }
}
