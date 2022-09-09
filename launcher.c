#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char *argv[]){
    int pid1, pid2, pid3, wpid;
    char chiamata[20];
    if(strcmp(argv[argc-2], "RBC") != 0){
        if ((pid1 = fork()) < 0) exit(EXIT_FAILURE); 
        else if (pid1 == 0) { /* child process */
            sprintf(chiamata, "./registro %s", argv[2]);
            system(chiamata);
            return 0;
        }
        if ((pid2 = fork()) < 0) exit(EXIT_FAILURE);
        else if (pid2 == 0) {
            sprintf(chiamata, "./treno %s", argv[1]);
            system(chiamata);
            return 0;
        }
    }
    else system("./RBC");
    return 0;
}