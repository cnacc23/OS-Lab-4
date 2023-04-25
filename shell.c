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
char *op = "";
bool pipes = false;
// variable that is 1 when the last command executed was the special command status
int last_command_status = 0;

char *redirection_lastOp = "";
char *inpF = "";
char *outF = "";
bool input = false;
bool output = false;
int numberPipes = 0;

// structure to store pipes command and options

struct Pipe
{
    char **arguments;
    struct Pipe *next;
};

struct Pipe *front = NULL;
struct Pipe *rear = NULL;

void enqueue(char *op[], int size)
{

    struct Pipe *nptr = malloc(sizeof(struct Pipe));
    nptr->arguments = (char **)malloc(size * sizeof(char *));

    for (int i = 0; i < size; i++)
    {
        (nptr->arguments)[i] = op[i];
    }

    nptr->next = NULL;
    if (front == NULL)
    {
        // queue is empty, set front and rear to new element
        front = nptr;
        rear = nptr;
    }
    else
    {
        // add new element to end of queue
        rear->next = nptr;
        rear = nptr;
    }
}

void freePipes()
{
    // printf("trying to free pipes :(\n");
    struct Pipe *current = front;

    while (current != NULL)
    {
        // free the memory allocated for the arguments array
        // free the memory allocated for the next pointer
        struct Pipe *next = current->next;
        free(current->arguments);
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
bool acceptToken(List *lp, char *ident)
{

    if (*lp != NULL && strcmp(((*lp)->t), ident) == 0)
    {
        *lp = (*lp)->next;
        return true;
    }
    return false;
}

/**
 * The fuunction returnSignal returns 1 if the last parsed command must not be executed.
 * It returns 0 if the command must be executed
 */
int returnSignal()
{
    if ((strcmp(lastOp, "&&") == 0) && (last != 0))
    {
        return 1;
    }
    else if ((strcmp(lastOp, "||") == 0) && (last == 0))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * The function parseExecutable parses an executable.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the executable was parsed successfully.
 */
bool parseExecutable(List *lp)
{

    // TODO: Determine whether to accept parsing an executable here.
    //
    // It is not recommended to check for existence of the executable already
    // here, since then it'll be hard to continue parsing the rest of the input
    // line (command execution should continue after a "not found" command),
    // it'll also be harder to print the correct error message.
    //
    // Instead, we recommend to just do a syntactical check here, which makes
    // more sense, and defer the binary existence check to the runtime part
    // you'll write later.
    optionsList = (char **)malloc(2 * sizeof(char *));
    optionsList[0] = (*lp)->t;
    (*lp) = (*lp)->next;

    return true;
}

/**
 * Checks whether the input string \param s is an operator.
 * @param s input string.
 * @return a bool denoting whether the current string is an operator.
 */
bool isOperator(char *s)
{
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

    for (int i = 0; operators[i] != NULL; i++)
    {
        if (strcmp(s, operators[i]) == 0)
        {
            op = operators[i];

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
bool parseOptions(List *lp)
{

    // TODO: store each (*lp)->t as an option, if any exist
    int i = 1;

    // storing each option in the array starting from postion 1
    int size_options = 2;
    while (*lp != NULL && !isOperator((*lp)->t))
    {
        if (i >= size_options - 1)
        {
            optionsList = realloc(optionsList, 2 * size_options * sizeof(char *));
            size_options = 2 * size_options;
        }
        optionsList[i] = (*lp)->t;
        (*lp) = (*lp)->next;
        i++;
    }
    optionsList[i] = NULL;
    size_options = i + 1;

    if (strcmp(op, "|") == 0 || pipes)
    {
        pipes = true;
        numberPipes++;
        enqueue(optionsList, size_options);
    }
    op = "";

    // exiting the shell if the command to be executed is "exit"
    if (strcmp(optionsList[0], "exit") == 0)
    {
        if (!returnSignal())
        {
            free(optionsList);
            _exit(0);
        }
        lastOp = "";
        free(optionsList);
    }

    // print the most recent exit code if the command executed is "status"
    else if (strcmp(optionsList[0], "status") == 0)
    {

        if (!returnSignal() || (last == 127))
        {
            last_command_status = 1;
            printf("The most recent exit code is: %d\n", last);
        }
        lastOp = "";
        free(optionsList);
    }

    else if (strcmp(optionsList[0], "cd") == 0)
    {

        char *gdir;
        char *dir;
        char *to;
        char buf[100];
        gdir = getcwd(buf, sizeof(buf));
        dir = strcat(gdir, "/");

        if (optionsList[1] == NULL)
        {
            printf("Error: cd requires folder to navigate to!\n");
            last = 2;
        }
        else
        {

            to = strcat(dir, optionsList[1]);
            last = 0;
            if (chdir(to) != 0)
            {
                printf("Error: cd directory not found!\n");
                last = 2;
            }
        }

        lastOp = "";
        free(optionsList);
    }
    if (pipes)
    {
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

    if (!parseCommand(lp))
    {
        return false;
    }

    if (acceptToken(lp, "|"))
    {
        return parsePipeline(lp);
    }

    return true;
}

/**
 * The function parseFileName parses a filename.
 * @param lp List pointer to the start of the tokenlist.
 * @return a bool denoting whether the filename was parsed successfully.
 */
bool parseFileName(List *lp)
{
    // TODO: Process the file name appropriately

    if (isEmpty(*lp) || isOperator((*lp)->t))
    {
        return false;
    }

    char *fileName = (*lp)->t;

    if (redirection_lastOp == "<")
    {
        inpF = fileName;
    }
    else if (redirection_lastOp == ">")
    {
        outF = fileName;
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
bool parseRedirections(List *lp)
{

    if (pipes)
    {

        if (acceptToken(lp, "<"))
        {
            fflush(stdout);
            redirection_lastOp = "<";
            if (!parseFileName(lp))
            {
                freePipes();
                return false;
            }
            input = true;
            if (acceptToken(lp, ">"))
            {
                redirection_lastOp = ">";
                if (parseFileName(lp))
                {
                    output = true;
                }
                else
                {
                    freePipes();
                    return false;
                }
            }
        }
        else if (acceptToken(lp, ">"))
        {
            redirection_lastOp = ">";
            if (!parseFileName(lp))
            {
                freePipes();
                return false;
            }
            output = true;
            if (acceptToken(lp, "<"))
            {
                redirection_lastOp = "<";
                if (parseFileName(lp))
                {
                    input = true;
                }
                else
                {
                    freePipes();
                    return false;
                }
            }
        }

        int fd[2];
        pid_t pid;
        int status;
        struct Pipe *curr = front;

        int prev_read = 0;

        for (int i = 0; i < numberPipes; i++)
        {
            // Create a pipe for inter-process communication
            pipe(fd);

            // Fork a child process
            pid = fork();

            if (pid == 0) // Child process
            {
                if (input)
                {
                    int fd_in = open(inpF, O_RDONLY);
                    if (fd_in == -1)
                    {
                        printf("Error in open\n");
                        return false;
                    }
                    close(0);
                    dup(fd_in);
                    close(fd_in);
                    input = false;
                }
                if (output)
                {
                    int fd_out = creat(outF, 0644);
                    if (fd_out == -1)
                    {
                        printf("Error in open\n");
                        return false;
                    }

                    close(1);
                    dup(fd_out);
                    close(fd_out);
                    output = false;
                }
                // Redirect stdin to read end of previous pipe
                if (i != 0)
                {
                    dup2(prev_read, STDIN_FILENO);
                    close(prev_read);
                }

                // Redirect stdout to write end of current pipe
                if (i != numberPipes - 1)
                {
                    dup2(fd[1], STDOUT_FILENO);
                    close(fd[1]);
                }

                // Execute the command
                execvp(curr->arguments[0], curr->arguments);

                // If execvp returns, there was an error
            }
            else if (pid < 0) // Error
            {
                perror("fork");
                exit(1);
            }

            // Parent process
            curr = curr->next;
            close(fd[1]);
            prev_read = fd[0];
        }

        // Wait for all child processes to complete

        for (int i = 0; i < numberPipes; i++)
        {

            waitpid(-1, &status, 0);
            last_command_status = 0;

            if (WIFEXITED(status))
            {
                // save exit code of the child process

                last = WEXITSTATUS(status);
            }
        }

        freePipes();

        return true;
    }

    if (isEmpty(*lp) || ((strcmp((*lp)->t, "<") != 0) && (strcmp((*lp)->t, ">") != 0)))
    {

        if (returnSignal() != 1)
        {

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
                if (returnSignal() == 1)
                {
                    lastOp = "";
                    free(optionsList);
                    _exit(0);
                }
                else
                {
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
                last_command_status = 0;

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

    input = false;
    output = false;

    if (acceptToken(lp, "<"))
    {
        fflush(stdout);

        redirection_lastOp = "<";
        if (!parseFileName(lp))
        {
            free(optionsList);
            return false;
        }
        input = true;
        if (acceptToken(lp, ">"))
        {
            redirection_lastOp = ">";
            if (parseFileName(lp))
            {
                output = true;
            }
            else
            {
                free(optionsList);
                return false;
            }
        }
    }
    else if (acceptToken(lp, ">"))
    {
        redirection_lastOp = ">";
        if (!parseFileName(lp))
        {
            free(optionsList);
            return false;
        }
        output = true;
        if (acceptToken(lp, "<"))
        {
            redirection_lastOp = "<";
            if (parseFileName(lp))
            {
                input = true;
            }
            else
            {
                free(optionsList);
                return false;
            }
        }
    }

    pid_t pid = fork();
    if (pid == 0)
    {

        if (input && output && (strcmp(inpF, outF) == 0))
        {
            printf("Error: input and output files cannot be equal!\n");
            _exit(2);
        }
        if (input)
        {
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
        }
        if (output)
        {
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
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        last_command_status = 0;

        if (WIFEXITED(status))
        {
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
bool parseBuiltIn(List *lp)
{

    //
    // TODO: Implement the logic for these builtins, and extend with
    // more builtins down the line
    //

    // NULL-terminated array makes it easy to expand this array later
    // without changing the code at other places.
    char *builtIns[] = {
        "exit",
        "status",
        "cd",
        NULL};

    for (int i = 0; builtIns[i] != NULL; i++)
    {
        if (acceptToken(lp, builtIns[i]))
        {
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
bool parseInputLine(List *lp)
{

    pipes = false;
    numberPipes = 0;
    if (isEmpty(*lp))
    {
        return true;
    }

    if (!parseChain(lp))
    {
        return false;
    }

    if (acceptToken(lp, "&") || acceptToken(lp, "&&"))
    {
        lastOp = "&&";
        return parseInputLine(lp);
    }
    else if (acceptToken(lp, "||"))
    {
        lastOp = "||";
        return parseInputLine(lp);
    }
    else if (acceptToken(lp, ";"))
    {
        lastOp = ";";
        return parseInputLine(lp);
    }

    return true;
}
