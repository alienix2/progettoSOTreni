#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, char *argv[]){
    int pid1, pid2, pid3, wpid;
    char chiamata[20];
    if(strcmp(argv[argc-2], "RBC") != 0){   //Se non viene richiesto RBC avvio registro e treno
        if ((pid1 = fork()) < 0) exit(EXIT_FAILURE); 
        else if (pid1 == 0) {   //Figlio 1 avvia registro
            int pid_file = open("/tmp/registro.pid", O_CREAT | O_RDWR, 0666);   //Apro il file di lock
            if(flock(pid_file, LOCK_EX | LOCK_NB) != 0){    //Se la flock da errore verifico che errore è
                if(errno == EWOULDBLOCK){   //Se l'errore è che il lock è in possesso di un altro processo non avvio registro
                    printf("Registro gia avviato, non è stata avviata un'altra istanza!\n");
                    exit(EXIT_FAILURE);
                }
            }   //Altrimenti si tratta di un errore non gestito e provo comunque ad avviare registro
            sprintf(chiamata, "./registro %s", argv[2]);
            system(chiamata);
            return 0;
        }
        if ((pid2 = fork()) < 0) exit(EXIT_FAILURE);
        else if (pid2 == 0) {   //Figlio 2 avvia treno
            sprintf(chiamata, "./treno %s", argv[1]);
            system(chiamata);
            return 0;
        }
    }
    else if ((pid1 = fork()) < 0) exit(EXIT_FAILURE);   //Altrimenti avvio RBC
    else if(pid1 == 0){
        system("./RBC");
    }
    return 0;
}