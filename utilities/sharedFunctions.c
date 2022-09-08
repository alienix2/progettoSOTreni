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

#include "sharedLibraries.h"

FILE *logFile;

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

void aggiungiLog(const char *fmt, ...){    //Aggiunge la stringa con parametri al giusto file di log
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

void accediSocket(int *server, struct sockaddr_un *serverAddress, int *serverLen, char* nomeServer){  //Funzione che inserisce i dati per accedere ad una socket partendo dai dati che riceve come parametri
    *serverLen = sizeof(*serverAddress);
    *server = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un tmp = {.sun_family = AF_UNIX};
    strcpy (tmp.sun_path, nomeServer);
    *serverAddress = tmp;
}

void connetti(int server, struct sockaddr *serverAddressPtr, int serverLen){   //Funzione che crea una connessione coi dati che riceve come parametri
    int risultato;
    do { /* Loop until a connection is made with the server */
        risultato = connect(server, serverAddressPtr, serverLen);
        if (risultato == -1){
            aggiungiLog("Connessione non riuscita con il server %s, riprovo in 3 secondi!", serverAddressPtr->sa_data);
	        sleep (3); /* Wait and then try again */
        }
    } while (risultato == -1);
    aggiungiLog("Connessione stabilita con il server %s", serverAddressPtr->sa_data);
}

void leggiElemento (int fd, char *str) {   //Funzione generica per ricevere un numero non definito di caratteri
    do {
        while(recv (fd, str, 1, 0) < 0){
            aggiungiLog("ERRORE: receive non andata a buon fine, riprovo fra 2 secondi");
            sleep(2);
       }
    }while (*str++ != '\0');
}