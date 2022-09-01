#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* for fork */
#include <sys/types.h> /* for pid_t */
#include <sys/wait.h> /* for wait */

int main(int argc, char *argv[]){
    int pid1, pid2, pid3, wpid;
    char chiamata[20];
    printf("%s", argv[argc-1]);
    if(strcmp(argv[argc-2], "RBC") != 0){
        if ((pid1 = fork()) < 0) exit(EXIT_FAILURE); 
        else if (pid1 == 0) { /* child process */
            sprintf(chiamata, "./registro %s", argv[2]);
            system(chiamata);
            exit;
        }
        if ((pid2 = fork()) < 0) exit(EXIT_FAILURE);
        else if (pid2 == 0) {
            sprintf(chiamata, "./treno %s", argv[1]);
            system(chiamata);
            exit;
        }
    }
    else system("./rbc");
    return 0;
}