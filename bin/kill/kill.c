#include <unistd.h>
#include <stdlib.h>
#include <sys/defs.h>
#include <stdio.h>
#include <dirent.h>
#include<signal.h>
int atoi(char *str)
{
    int res = 0; // Initialize result
  
    // Iterate through all characters of input string and
    // update result
    for (int i = 0; str[i] != '\0'; ++i)
        res = res*10 + str[i] - '0';
  
    // return result.
    return res;
}

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
