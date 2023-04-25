#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "scanner.h"
#include "shell.h"


// array to store commands
char **optionsList;

// int to store last status and special status 
int last=0;
int specialStatus= 0;   // 1 when last command exits w special status 

//string that stores current and last operators being parsed
char *lastOp= "";
char *currOp= "";

//variables for piping 
bool containsPipes= false;
int numPipes= 0;

//variables for pipes and redirections
char *redirection= "";
char *inpF= "";
char *outF= "";
bool isInput= false;
bool isOutput= false;

// structure to store pipes command and options
struct Pipe
{
    char **cmds;
    struct Pipe *next;
};

struct Pipe *head = NULL;
struct Pipe *tail = NULL;

// function to enqueue piping data into struct 
void enqueue(char *args[], int size){
    struct Pipe *newNode = malloc(sizeof(struct Pipe));
    newNode->cmds = (char **)malloc(size * sizeof(char *));

    for (int i = 0; i < size; i++){
        (newNode->cmds)[i] = args[i];
    }

    newNode->next = NULL;

    //if LL is empty, initialize with newNode at head
    if (head == NULL){
        head = newNode;

    // else add node to end of LL
    }else{
        tail->next = tail;
        tail = newNode;
    }
}

// function to free memory allocated when piping 
void freePipes(){
    struct Pipe *current = head;

    while (current != NULL){

        struct Pipe *next = current->next;
        free(current->cmds);
        free(current);
        current = next;
    }
    head = NULL;
    tail = NULL;
}

/**
 * The function acceptToken checks whether the current token matches a target identifier,
 * and goes to the next token if this is the case.
 * @param lp List pointer to the start of the tokenlist.
 * @param ident target identifier
 * @return a bool denoting whether the current token matches the target identifier.
 */
bool acceptToken(List *lp, char *ident){
    if (*lp != NULL && strcmp(((*lp)->t), ident) == 0){
        *lp = (*lp)->next;
        return true;
    }
    return false;
}

//function returns wether or not next command should be executed based on the grammar
bool executeNextCommand(){

    if ((strcmp(lastOp, "&") == 0) && (last != 0)){
        return 1;

    }else if ((strcmp(lastOp, "||") == 0) && (last == 0)){
        return 1;

    }else{
        return 0;
    }
}

/**
 * The function parseExecutable parses an executable.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the executable was parsed successfully.
 */
bool parseExecutable(List *lp){

    //dynamically allocate memory for optionsList (stores command options)
    optionsList = (char **)malloc(2 * sizeof(char *));
    optionsList[0] = (*lp)->t;

    //increment pointer 
    (*lp) = (*lp)->next;

    return true;
}

/**
 * Checks whether the input string \param s is an operator.
 * @param s input string.
 * @return a bool denoting whether the current string is an operator.
 */
bool isOperator(char *s){
    // NULL-terminated array makes it easy to expand this array later
    // without changing the code at other places.
    char *operators[] = {
        "&",
        "&&",
        "||",
        ";",
        "<",
        ">",
        "|",
        NULL};

    for (int i = 0; operators[i] != NULL; i++){
        if (strcmp(s, operators[i]) == 0){
            currOp = operators[i];  // store current operator 
            return true;
        }
    }
    return false;
}

/**
 * The function parseOptions parses options.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the options were parsed successfully.
 */
bool parseOptions(List *lp){

    //store each (*lp)->t as an option, if any exist
    int i = 1;
    int opListSize = 2;     //current options list size 

    // iterate through tokenlist and storing each option in array
    while (*lp != NULL && !isOperator((*lp)->t)){

         // reallocate memory to expand optionsList if needed
        if (i >= opListSize - 1){
            optionsList = realloc(optionsList, 2 * opListSize * sizeof(char *));
            opListSize = 2 * opListSize;
        }

        //storing each (*lp)-> as an option
        optionsList[i] = (*lp)->t;
        (*lp) = (*lp)->next;
        i++;
    }

    //once each option is stored, set its index to NULL and increment the opListSize
    optionsList[i] = NULL;
    opListSize = i + 1;


    // exiting the shell if the command to be executed is "exit"
    if (strcmp(optionsList[0], "exit") == 0){
        if (!executeNextCommand()){
            free(optionsList);
            _exit(0);
        }
        lastOp = "";
        free(optionsList);
    }

    // print the most recent exit code if the command executed is "status"
    else if (strcmp(optionsList[0], "status") == 0){

        if (!executeNextCommand() || (last == 127)){
            //last_command_status = 1;
            printf("The most recent exit code is: %d\n", last);
        }
        lastOp = "";
        free(optionsList);


    //check input directory given for cd 
    }else if (strcmp(optionsList[0], "cd") == 0){
        if (executeNextCommand()){
            if(optionsList[1] == NULL){
                printf("Error: cd requires folder to navigate to!\n");
                last= 2;        // exit 2 for cd errors 

            } else {
            // check directory exists 
                if(chdir(optionsList[1]) == -1){
                    printf("Error: cd directory not found!\n");
                    last= 2;
                
                }else{
                last= 0;
                }
            }
        }
        free(optionsList);
        lastOp= "";
    }

    if (strcmp(currOp, "|") == 0 || containsPipes){
        containsPipes = true;
        numPipes++;
        enqueue(optionsList, opListSize);
    }
    currOp = "";

    if (containsPipes){
        free(optionsList);
    }

    return true;
}

/**
 * The function parseRedirections parses a command according to the grammar:
 *
 * <command>        ::= <executable> <options>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the command was parsed successfully.
 */
bool parseCommand(List *lp)
{
    return parseExecutable(lp) && parseOptions(lp);
}

/**
 * The function parsePipeline parses a pipeline according to the grammar:
 *
 * <pipeline>           ::= <command> "|" <pipeline>
 *                       | <command>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the pipeline was parsed successfully.
 */
bool parsePipeline(List *lp)
{

    if (!parseCommand(lp)){
        return false;
    }

    if (acceptToken(lp, "|")){
        return parsePipeline(lp);
    }

    return true;
}

/**
 * The function parseFileName parses a filename.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the filename was parsed successfully.
 */
bool parseFileName(List *lp){
    
    if (isEmpty(*lp) || isOperator((*lp)->t)){
        return false;
    }

    // store pointer 
    char *fileName = (*lp)->t;

    //assign file name to input/output file place holder

    if (strcmp(redirection,"<") == 0){
        inpF = fileName;

    }else if (strcmp(redirection, ">") == 0){
        outF= fileName;
    }

    *lp = (*lp)->next;
    return true;
}

/**
 * The function parseRedirections parses redirections according to the grammar:
 *
 * <redirections>       ::= <pipeline> <redirections>
 *                       |  <builtin> <options>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the redirections were parsed successfully.
 */
bool parseRedirections(List *lp){

    // first handle piping 
    if (containsPipes){


        if(acceptToken(lp, ">")){
            lastOp= ">"; 

            // check for valid input file 
            if(parseFileName(lp)){
                isInput= true;
            } else {
                freePipes();
                return false;
            }

            // check for valid output file 
            if(acceptToken(lp, "<")){
                isOutput= true;
            } else {
                freePipes();
                return false;
            }


        // repeat process for output redirection
        } else if (acceptToken(lp, "<")){
            fflush(stdout);
            lastOp = "<";

            if (parseFileName(lp)){
                isInput= true;
            }else {
                freePipes();
                return false;
            }
          

            if (acceptToken(lp, ">")){
                redirection = ">";
                if (parseFileName(lp)){
                    isOutput = true;
                }else{
                    freePipes();
                    return false;
                }
            }

        }
    
        //now execute commands with pipes 

        // current command node (starting at head of LL)
        struct Pipe *curr = head;

        int fileDescriptor[2];
        pid_t pid;
        int status, openInp, openOut;
        int prevCmd = 0;

        for (int i = 0; i < numPipes; i++){
            
            

            //parent process creates pipe 
            pipe(fileDescriptor);

            // then parent forks a child process 
            pid = fork();

            if(pid == -1){
                printf("Error in the fork\n");
                last= 1;
                exit(1);

        
            }else if (pid == 0){

                //for valid input that exists 
                if (isInput){

                    openInp= open(inpF, O_RDONLY); //pgm using file's data as input, so read only
                    
                    //cmd success
                    if (openInp == 0){
                        close(0);
                        dup(openInp);
                        close(openInp);
                        //isInput= false

                    //cmd failure
                    }else{
                        printf("Error opening file\n");
                        return false;
                    }
                }

                // for valid output
                if (isOutput){
                    openOut = open(outF, O_CREAT| O_TRUNC| O_WRONLY, 0644); //create file if it doesn't exist, truncate if it does
                    
                    //cmd success
                    if (openOut == 0){
                        close(1);
                        dup(openOut);
                        close(openOut);
                        //isOutput = false;

                    // cmd failure
                    } else {
                        printf("Error opening file\n");
                        return false;
                    }
                }


                // redirecting stdin to read 
                if (i != 0){
                    dup2(prevCmd, STDIN_FILENO); //redirect stdin to file 
                    close(prevCmd);    // reallocate pointer 
                }

                // redirecting stdout to write 
                if (i != numPipes - 1){
                    dup2(fileDescriptor[1], STDOUT_FILENO); //redirect stdout to file
                    close(fileDescriptor[1]);
                }


                // execute command 
                execvp(curr->cmds[0], curr->cmds);

            }

            // increment curr to get to next pipe command 
            curr = curr->next;
            close(fileDescriptor[1]);
            prevCmd = fileDescriptor[0];
        }

       
        // wait for all child processes to finish
        for (int i = 0; i < numPipes; i++){

            waitpid(-1, &status, 0);
           // last_command_status = 0;

            //determine if child process ended normally 
            if (WIFEXITED(status)){
                last = WEXITSTATUS(status); // if so, set last to child process' exit 
            }
        }

        freePipes();
        return true;
    }


     //now handle commands w/o pipes

    bool noRedirections= (strcmp((*lp)->t, ">") != 0) && (strcmp((*lp)->t, "<") != 0);

    if (isEmpty(*lp) || noRedirections){

        //check if command should be executed 
        if (executeNextCommand()){

            pid_t pid = fork();

            if (pid < 0){
              
                printf("Error in fork");
                last= 1; 
                exit(1);


            }else if (pid == 0){

                fflush(stdout);

                // check if the command should be executed
                if (executeNextCommand()){

                    //use execvp to execute command 
                    if(execvp(optionsList[0], optionsList) < 0){
                        
                        //if execvp fails
                        printf("Error: command not found!\n");
                        last= 127;
                        _exit(127);
                    }
                    
                }else{
                    // if command should not be executed 
                    lastOp = "";
                    free(optionsList);
                    _exit(0);
                }

            // waiting for child processes to finish         
            }else{
                
                int status; 

                if(wait(&status) < 0){
                    last= 1;
                    _exit(1);   //wait for child to terminate 
                }else {
                    last= 0;
                }


                waitpid(pid, &status, 0);
                //last_command_status = 0;


                //determine if child process ended naturally 

                if (WIFEXITED(status)){
                    // save exit code of the child process

                    last = WEXITSTATUS(status);
                }
            }
        }

        lastOp = "";
        free(optionsList);

        return true;
    }

    isInput = false;
    isOutput = false;

    if (acceptToken(lp, "<")){
        fflush(stdout);

        redirection = "<";
        if (!parseFileName(lp)){
            free(optionsList);
            return false;
        }
        isInput = true;
        if (acceptToken(lp, ">")){
            redirection = ">";
            if (parseFileName(lp)){
                isOutput = true;
            }else{
                free(optionsList);
                return false;
            }
        }
    }else if (acceptToken(lp, ">")){
        redirection = ">";
        if (!parseFileName(lp)){
            free(optionsList);
            return false;
        }
        isOutput = true;
        if (acceptToken(lp, "<")){
            redirection = "<";
            if (parseFileName(lp)){
                isInput = true;
            }else{
                free(optionsList);
                return false;
            }
        }
    }

    pid_t pid = fork();
    if (pid == 0){

        if (isInput && isOutput && (strcmp(inpF, outF) == 0)){
            printf("Error: isInput and isOutput files cannot be equal!\n");
            _exit(2);
        }

        if (isInput){
            int fd_in = open(inpF, O_RDONLY);
            if (fd_in == -1)
            {
                printf("Error in open\n");
                return false;
                // what do we do, we return false ?
            }
            close(0);
            dup(fd_in);
            close(fd_in);


        }if (isOutput){
            int fd_out = creat(outF, 0644);
            if (fd_out == -1)
            {
                printf("Error in open\n");
                return false;
                // what do we do, we return false ?
            }

            close(1);
            dup(fd_out);
            close(fd_out);
        }

        execvp(optionsList[0], optionsList);
        printf("Error: command not found!\n");
        _exit(127);

    }else{
        int status;
        waitpid(pid, &status, 0);
       // last_command_status = 0;

        if (WIFEXITED(status)){
            // save exit code of the child process
            last = WEXITSTATUS(status);
        }
    }
    free(optionsList);
    return true;
}

/**
 * The function parseBuiltIn parses a builtin.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the builtin was parsed successfully.
 */
bool parseBuiltIn(List *lp){

    // NULL-terminated array makes it easy to expand this array later
    // without changing the code at other places.
    char *builtIns[] = {
        "exit",
        "status",
        "cd",
        NULL};

    for (int i = 0; builtIns[i] != NULL; i++){
        if (acceptToken(lp, builtIns[i])){
            optionsList = (char **)malloc(2 * sizeof(char *));
            optionsList[0] = builtIns[i];
            return true;
        }
    }

    return false;
}

/**
 * The function parseChain parses a chain according to the grammar:
 *
 * <chain>              ::= <pipeline> <redirections>
 *                       |  <builtin> <options>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the chain was parsed successfully.
 */
bool parseChain(List *lp)
{

    if (parseBuiltIn(lp)){
        return parseOptions(lp);
    }

    if (parsePipeline(lp)){
        return parseRedirections(lp);
    }
    return false;
}

/**
 * The function parseInputLine parses an inputline according to the grammar:
 *
 * <inputline>      ::= <chain> & <inputline>
 *                   | <chain> && <inputline>
 *                   | <chain> || <inputline>
 *                   | <chain> ; <inputline>
 *                   | <chain>
 *                   | <empty>
 *
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the inputline was parsed successfully.
 */
bool parseInputLine(List *lp){

    containsPipes = false;
    numPipes = 0;
    if (isEmpty(*lp)){
        return true;
    }

    if (!parseChain(lp)){
        return false;
    }

    if (acceptToken(lp, "&") || acceptToken(lp, "&&")){
        lastOp = "&&";
        return parseInputLine(lp);
    }else if (acceptToken(lp, "||")){
        lastOp = "||";
        return parseInputLine(lp);
    }else if (acceptToken(lp, ";")){
        lastOp = ";";
        return parseInputLine(lp);
    }
    return true;
}