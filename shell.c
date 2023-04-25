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

// array to store command options
char **optionsList;

// variable for the exit code of the last command executed
int last = 0;

// variable to keep track of the last parsed operator
char *lastOp = "";
char *currOp = "";
bool containsPipes= false;
// variable that is 1 when the last command executed was the special command status
//int last_command_status = 0;

char *inpF = "";
char *outF = "";
bool isInput = false;
bool isOutput = false;
int numPipes = 0;

// structure to store pipes command and options

struct Pipe
{
    char **cmds;
    struct Pipe *next;
};

struct Pipe *front = NULL;
struct Pipe *rear = NULL;


//function to add and store pipes and commands to LL
void enqueue(char *args[], int size){

    struct Pipe *newNode = malloc(sizeof(struct Pipe));
    newNode->cmds = (char **)malloc(size * sizeof(char *));

    for (int i = 0; i < size; i++){
        (newNode->cmds)[i] = args[i];
    }

    newNode->next = NULL;
    if (front == NULL){
        // queue is empty, set front and rear to new element
        front = newNode;
        rear = newNode;
    }else{
        // add new element to end of queue
        rear->next = newNode;
        rear = newNode;
    }
}

void freePipes(){
    // printf("trying to free pipes :(\n");
    struct Pipe *current = front;

    while (current != NULL){
        struct Pipe *next = current->next;
        free(current->cmds);
        free(current);
        current = next;
    }
    front = NULL;
    rear = NULL;
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

//determines whether or not to execute next command 
int executeNextCommand(){
    if ((strcmp(lastOp, "&&") == 0) && (last != 0)){
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

    //dynamically allocate memory for optionsList and store first pointer at first index 
    optionsList = (char **)malloc(2 * sizeof(char *));
   
    int i= 0;

    optionsList[i] = (*lp)->t;
    (*lp) = (*lp)->next;    // increment pointer 

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
            currOp = operators[i];

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

    int i = 1;
    int opListSize = 2; //current optionsList size 


    //storing each (*lp)->t as an option, if any exist
    while (*lp != NULL && !isOperator((*lp)->t)){

        //reallocating memory to expand optionsList if needed
        if (i >= opListSize - 1){
            optionsList = realloc(optionsList, 2 * opListSize * sizeof(char *));
            opListSize = 2 * opListSize;
        }

        optionsList[i] = (*lp)->t;
        (*lp) = (*lp)->next;
        i++;
    }
    optionsList[i] = NULL;
    opListSize = i + 1;

    //pipe logic 
    if (strcmp(currOp, "|") == 0 || containsPipes){
        containsPipes = true;
        numPipes++;

        //add to LL 
        enqueue(optionsList, opListSize);
    }
    currOp = "";

    //built-ins logic 

    //to exit shell
    if (strcmp(optionsList[0], "exit") == 0){
        if (!executeNextCommand()){
            free(optionsList);
            _exit(0);   //child processes to use 
        }
        lastOp = "";
        free(optionsList);
    

    // print most recent status 
    }else if (strcmp(optionsList[0], "status") == 0){

        if (!executeNextCommand() || (last == 127)){
            //last_command_status = 1;
            printf("The most recent exit code is: %d\n", last);
        }
        lastOp = "";
        free(optionsList);


    //check input for directory given for cd 
    }else if (strcmp(optionsList[0], "cd") == 0){

        if (executeNextCommand() == 0){

            if(optionsList[1] == NULL){
                printf("Error: cd requires folder to navigate to!\n");
                last= 2;        // exit 2 for cd errors 

            } else {

            // check directory exists 
                if(chdir(optionsList[1]) == -1){
                    printf("Error: cd directory not found!\n");
                    last= 2;
    
                }
                last= 0;
            }
        }
        lastOp = "";
        free(optionsList);
    }
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
bool parseCommand(List *lp){
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
bool parsePipeline(List *lp){

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

    if (isEmpty(*lp) || isOperator((*lp)->t))return false;
    
    // save file name and assign to input/output file depending on redirection symbol
    char *fileName = (*lp)->t;

    if (strcmp(lastOp, "<") == 0){
        inpF = fileName;
    }else if (strcmp(lastOp, ">") == 0){
        outF = fileName;
    }

    //inc pointer
    *lp = (*lp)->next;
    return true;
}


bool checkRedirections(List *lp){
    
    if (acceptToken(lp, "<")){
            fflush(stdout);
            lastOp = "<";


            if (parseFileName(lp)){
                isInput= true;
            }else{
                freePipes();
                return false;
            }

            if (acceptToken(lp, ">")){
                lastOp = ">";


                if (parseFileName(lp)){
                    isOutput = true;
                }else {
                    freePipes();
                    return false;
                }
            }
        }
        else if (acceptToken(lp, ">")){
            lastOp = ">";

            if (parseFileName(lp)){
                isOutput= true;
            }else {
                freePipes();
                return false;
            }
        
            if (acceptToken(lp, "<")){
                //lastOp = "<";
                
                if (parseFileName(lp)){
                    lastOp= "<";
                    isInput = true;
                }else{
                    freePipes();
                    return false;
                }
            }
        }
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

    if (containsPipes){

       checkRedirections(lp);

        int pipefd[2];  //fd[0] for input (read end), fd[1] for output (write end)
        pid_t pid;
        int status;
        int openIn, openOut;
        struct Pipe *curr = front;

        int prev_read = 0;

        for (int i = 0; i < numPipes; i++){
            // Create a pipe for inter-process communication
            pipe(pipefd);

            // Fork a child process
            pid = fork();

            if (pid == 0){ // Child process{
                if (isInput){
                    openIn = open(inpF, O_RDONLY);  //pgm using input file's data so read only 

                    if(openIn == 0){
                        close(0);       // deallocates input fd 
                        dup(openIn);
                        close(openIn);
                        //isInput= false;

                    } else if (openIn == -1){
                        printf("Error opening input file\n");
                        return false;
                    }
                }
                
                if (isOutput){
                    openOut = open(outF, O_CREAT| O_TRUNC| O_WRONLY, 0644); // create file if it doesn't exits, truncate if it does
                    
                    if(openOut == 0){
                        close(1);       //deallocates output fd
                        dup(openOut);
                        close(openOut);
                        //isOutput= false;

                    }else if (openOut == -1){
                        printf("Error opening output file\n");
                        return false;
                    }
                }


                // Redirect stdin to read end of previous pipe
                if (i != 0){
                    dup2(prev_read, STDIN_FILENO);
                    close(prev_read);
                }

                // Redirect stdout to write end of current pipe
                if (i != numPipes - 1){
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                // Execute the command
                execvp(curr->cmds[0], curr->cmds);

                // If execvp returns, there was an error
            }else if (pid < 0){
                printf("Error in fork\n"); 
                last= 1;
                exit(1);
            }

            // Parent process
            curr = curr->next;
            close(pipefd[1]);
            prev_read = pipefd[0];
        }

        
        //waiting for all child processes to finish
        for (int i = 0; i < numPipes; i++){

            waitpid(-1, &status, 0);
            //last_command_status = 0;

            // check if child process ended normally 
            if (WIFEXITED(status)){
                last = WEXITSTATUS(status); // if so, save child process' exit code 
            }
        }

        freePipes();
        return true;
    }

    if (isEmpty(*lp) || ((strcmp((*lp)->t, "<") != 0) && (strcmp((*lp)->t, ">") != 0)))
    {

        if (executeNextCommand() != 1){

            pid_t pid = fork();

            if (pid == -1)
            {
                // Handle error.
                printf("error in fork");
            }
            else if (pid == 0)
            {
                // We are in the child process.

                fflush(stdout);

                // check if the command should be executed
                if (executeNextCommand() == 1){
                    lastOp = "";
                    free(optionsList);
                    _exit(0);
                }else{
                    // Use execvp() to execute the command in the child process.

                    execvp(optionsList[0], optionsList);
                    // If execvp() succeeds, this code will not be reached.
                    printf("Error: command not found!\n");
                    _exit(127);
                }
            }
            else
            {
                // We are in the parent process.
                // Use waitpid() to wait for the child process to complete.

                int status;
                waitpid(pid, &status, 0);
                //last_command_status = 0;

                if (WIFEXITED(status))
                {
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

    checkRedirections(lp); 

    pid_t pid = fork();
    if (pid == 0){
        
        //check if input and output files are the same 
        if (isInput && isOutput && (strcmp(inpF, outF) == 0)){
            printf("Error: input and output files cannot be equal!\n");
            _exit(2);
        }
        
        if (isInput){
            int openIn = open(inpF, O_RDONLY); // pgm taking input from file so read only 
            
            if (openIn < 0){
                printf("Error in open\n");
                return false;
            
            } else {
                close(0);
                dup(openIn);        //redirect input
                close(openIn);
        
            }
        }

        if (isOutput){
            int openOut = open(outF, O_CREAT| O_TRUNC| O_WRONLY, 0644); // create file if it doesn't exist, truncate if it does 
            if (openOut < 0){
                printf("Error in open\n");
                return false;

            }else{
                close(1);
                dup(openOut);   // redirect output
                close(openOut);
            }
        }

        // child process running user command 
        if(execvp(optionsList[0], optionsList) < 0){

            //upon execvp failure 
            printf("Error: command not found!\n");
            last= 127;
            _exit(127);
        }


    }else{

        //wait for child processes to exit 
        int status;
        waitpid(pid, &status, 0);
        //last_command_status = 0;

        //determine if child processes exit natrually
        // if so, save child process' exit code
        if (WIFEXITED(status)) last = WEXITSTATUS(status); 

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
        NULL
        };

    for (int i = 0; builtIns[i] != NULL; i++){
        if (acceptToken(lp, builtIns[i])){

            //dynamically allocate memory for optionsList
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

    // pipes and redirections not parsed here
    containsPipes = false;
    numPipes = 0;

    if (isEmpty(*lp))return true;

    if (!parseChain(lp))return false;
    
    // save last operator 
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
