#include <unistd.h>
#include <stdlib.h>
#include <sys/defs.h>
#include <stdio.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>


int main(int argc, char *argv[], char *envp[]) {	

        if(argc < 2) {
                puts("signal/pid not specified\n");
        }
        else {
               //TODO: char *signal = argv[1];
                char *pid = argv[2];
		int pid_t = atoi(pid);
		if(!kill(pid_t,9))
			puts("kill return error");	

        }
 	
	return 0;
}

