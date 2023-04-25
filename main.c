#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>  
#include <sys/wait.h>

#include "scanner.h"
#include "shell.h"

int main(int argc, char *argv[]) {
    char *inputLine;
    List tokenList;
    List t;
   
   //so things print in order 
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);


   
    while (true) {
        inputLine = readInputLine();

        if(!inputLine){
            break;
        }

        tokenList = getTokenList(inputLine);
        t= tokenList;

        bool parsedSuccessfully = parseInputLine(&tokenList);
        
      
        if (tokenList == NULL && parsedSuccessfully) {

            // Input was parsed successfully and can be accessed in "tokenList"

            // However, this is still a simple list of strings, it might be convenient
            // to build some intermediate structure representing the input line or a
            // command that you then construct in the parsing logic. It's up to you
            // to determine how to approach this!
        } else {
            printf("Error: invalid syntax!\n");
            exit(1);
        }

        free(inputLine);
        freeTokenList(t);

        }
    
    return 0;
}