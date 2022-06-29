#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* For AFUNIX sockets */
#define DEFAULT_PROTOCOL 0

//Registro di PROVA, ToDo quello vero

int receiveNumero(int fd) {
  int numero;
  recv(fd, &numero, sizeof(numero), 0);
  numero = ntohl(numero);
  printf("Ho ricevuto %d, PID:%d\n", numero, getpid());
}

int sendItinerario(int fd, char **itinerario){
  for(int i = 0; i<7; i++){
    send(fd, itinerario[i], strlen(itinerario[i])+1, 0);
  }
  fflush(fd);
  return 0;
}

int main (void) {
  int registro, clientFd, serverLen, clientLen;
  struct sockaddr_un registroAddress; /*Server address */
  struct sockaddr* serverSockAddrPtr; /*Ptr to server address*/
  struct sockaddr_un clientUNIXAddress; /*Client address */
  struct sockaddr* registroAddressPtr;/*Ptr to client address*/

  char *itinerario[7];
  int random = rand();
  itinerario[0] = "S0";
  itinerario[1] = "MA1";
  itinerario[2] = "MA2";
  itinerario[3] = "MA3";
  itinerario[4] = "MA4";
  itinerario[5] = "MA5";
  itinerario[6] = "S1";

  serverSockAddrPtr = (struct sockaddr*) &registroAddress;
  serverLen = sizeof (registroAddress);
  registroAddressPtr = (struct sockaddr*) &clientUNIXAddress;
  clientLen = sizeof (clientUNIXAddress);
  
  registro = socket (AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL);
  registroAddress.sun_family = AF_UNIX; /* Set domain type */
  strcpy (registroAddress.sun_path, "RegistroTreni"); /* Set name */
  unlink ("RegistroTreni"); /* Remove file if it already exists */
  bind (registro, serverSockAddrPtr, serverLen);/*Create file*/
  listen (registro, 5); /* Maximum pending connection length */
  while (1) {/* Loop forever */ /* Accept a client connection */
    clientFd = accept (registro, registroAddressPtr, &clientLen);
    if (fork () == 0) { /* Create child to send receipe */
      receiveNumero(clientFd);
      sendItinerario(clientFd, &itinerario);
    } else
      close (clientFd); /* Close the client descriptor */
  }
}
