// If you are not compiling with the gcc option --std=gnu99, then
// uncomment the following line or you might get a compiler warning
//#define _GNU_SOURCE

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>


// define constants

#define MAX_ARGS 256
#define INPUTLENGTH 2048

// create string array to store argument elements

char *args[MAX_ARGS];

// keep track of number of arguments

int num_args = 0;

// to track child process status

int childStatus;

int   processes[100];	// Array of all process's pids
int   num_proc = 0;			// Total number of forked processes

// track background request and foreground only mode

int bgFlag = 0;
int foregroundOnly = 0;

// Sigaction structs
struct sigaction SIGINTAction;	// SIGINT handler
struct sigaction SIGTSTPAction; // SIGTSTP handler


void handle_SIGTSTP() {
    char* message;		
    int messageSize;	
    char* prompt = ": "; 

    if (foregroundOnly == 0) {
        message = "\nEntering foreground-only mode (& is now ignored)\n";
        messageSize = 50;
        foregroundOnly = 1;
    }
    else {
        message = "\nExiting foreground-only mode\n";
        messageSize = 30;
        foregroundOnly = 0;
    }
        
    
    
    // Using write reentrant function for custom signal handler
    write(STDOUT_FILENO, message, messageSize);
    write(STDOUT_FILENO, prompt, 2);
}

void callExit()
{
    // Exit the program without terminating any processes
    if (num_proc == 0) {
        exit(0);
    }
    // There are processes that must be terminated 
    else {
        for (int i = 0; i < num_proc; i++) {
            kill(processes[i], SIGTERM);
        }
        exit(1);
    }
}
void callCD() 
{
    
    int flag;
    char curDir[256];

    // user did not enter a directory
    if (num_args == 1) {
        flag = chdir(getenv("HOME"));
    }
    else {
        flag = chdir(args[1]);
    }
    if (flag == 0) {
        printf("%s\n", getcwd(curDir, 256));
    }
    else {
        printf("cd failed\n");
    }
    fflush(stdout);
}

// pass in the childStatus
void callStatus(int status)
{
    int errStatus;
    // Exited normally
    if (WIFEXITED(status)) {
        errStatus = WEXITSTATUS(status);
        printf("exit value %d\n", errStatus);
    }
    // Exited due to signal
    else {
        errStatus = WTERMSIG(status);
        printf("terminated with signal %d\n", errStatus);
    }
    fflush(stdout);
}



void childProcess()
{
    
    
    // initalize file redirect variables
    int haveInput = 0, haveOutput = 0;
    char inputFile[INPUTLENGTH], outputFile[INPUTLENGTH];

    // check for redirect operators
    for (int i = 0; args[i] != NULL; i++) {
       
        //input file check
        if (strcmp(args[i], "<") == 0) {
            haveInput = 1;
            args[i] = NULL;
            strcpy(inputFile, args[i + 1]);
            i++;
            
        }
        //output file check
        else if (strcmp(args[i], ">") == 0){
            haveOutput = 1;
            args[i] = NULL;
            strcpy(outputFile, args[i + 1]);
            i++;
            
        }
    }

    if (haveInput == 1) {

        // if in background mode and no file specified

        if (bgFlag == 1 && inputFile == NULL) {

            // pass input to /dev/null
            int sourceFD = open("/dev/null", O_RDONLY);
            if (sourceFD == -1) {
                perror("source open()");
                exit(1);
            }

            // Redirect stdin to source file
            int result = dup2(sourceFD, 0);
            if (result == -1) {
                perror("source dup2()");
                exit(2);
            }
        }

        else
        {

            // Open source file
            int sourceFD = open(inputFile, O_RDONLY);
            if (sourceFD == -1) {
                perror("source open()");
                exit(1);
            }

            // Redirect stdin to source file
            int result = dup2(sourceFD, 0);
            if (result == -1) {
                perror("source dup2()");
                exit(2);
            }

        }

    }

    if (haveOutput == 1) {

        // if in background mode and no file specified

        if (bgFlag == 1 && outputFile == NULL) {
            // pass output to /dev/null
            int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (targetFD == -1) {
                perror("target open()");
                exit(1);
            }

            // Redirect stdout to target file
            int result = dup2(targetFD, 1);
            if (result == -1) {
                perror("target dup2()");
                exit(2);
            }

        }

        else
        {

            // Open target file
            int targetFD = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (targetFD == -1) {
                perror("target open()");
                exit(1);
            }

            // Redirect stdout to target file
            int result = dup2(targetFD, 1);
            if (result == -1) {
                perror("target dup2()");
                exit(2);
            }

        }
     
    }

    // Give CTRL-C handler control so foreground processes can be terminated
    if (bgFlag == 0) {
        SIGINTAction.sa_handler = SIG_DFL;
    }
    sigaction(SIGINT, &SIGINTAction, NULL);

    // execute command
    if (execvp(args[0], args) == -1) {
        perror(args[0]);
        exit(1);
    }
    
}

void parentProcess(pid_t pid)
{
    // if backgroud process make it non-blocking
    if (bgFlag == 1) {
        waitpid(pid, &childStatus, WNOHANG);
        printf("background pid is %d\n", pid);
        fflush(stdout);
    }

    // else make it blocking
    else {

        waitpid(pid, &childStatus, 0);

    }

}

void callOtherCmds()
{

    // check for background process
    if (strcmp(args[num_args - 1], "&") == 0) {
        // set background flag if not in foreground only mode
        if (foregroundOnly == 0) {
            bgFlag = 1;
        }
        // delete &
        args[num_args - 1] = NULL;
    }
    
    pid_t pid = fork();
    processes[num_proc] = pid;	
    num_proc++;

    switch (pid) {

    case -1: // Error
        perror("fork() failed\n");
        exit(1);
        break;

    case 0:  // Child
        childProcess();
        break;

    default: // Parent
        parentProcess(pid);
    }

    // to inform of completion and status of bg pid
    while ((pid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
        printf("background pid %d is done: ", pid);
        fflush(stdout);
        callStatus(childStatus);
    }
}



void getCommand()
{
    // get the user input
    char argString[INPUTLENGTH];
    int pos = 0;
    printf(": ");
    fflush(stdout);
    fgets(argString, INPUTLENGTH, stdin);

    if (argString[0] != '\n') {
        // take out the new line character if it's not the first character
        strtok(argString, "\n");
    }

    // perform variable expansion before token
    char tempStr[INPUTLENGTH];
    char pidStr[30];
    sprintf(pidStr, "%d", getpid());
    int i = 0;
    int k = 0;
    int j = strlen(argString);
    int pid_leng = strlen(pidStr);
    
    while (i < j) {
        int a = 0;
        if (argString[i] == '$' && argString[i + 1] == '$') {
            while (a < pid_leng) {
                tempStr[k] = pidStr[a];
                k++;
                a++;
            }
            i = i + 2;
        }
        else {
            tempStr[k] = argString[i];
            k++;
            i++;
        }
    }
    tempStr[k] = '\0';
    
    // user input now stored in tempString, store elements in string array "args"
    char* token;

    token = strtok(tempStr, " ");
    

    while (token != NULL) {

        args[pos] = token;
        token = strtok(NULL, " ");
        pos++;
    }
    num_args = pos;

    return;

}

void parseCommand()
{
    // first check if user didn't enter anything or entered a comment
    if (args[0][0] == '\n' || args[0][0] == '#') {
        
        // return to main loop
        return;
    }
    // perform the $$ variable expansion for each element

    int i, j;
    char tempStr[INPUTLENGTH];
   
    for (i = 0; i < num_args; i++) {

        for (j = 0; j < strlen(args[i]); j++) {
            if (args[i][j] == '$' && args[i][j + 1] == '$') {
                
                args[i][j] = '\0';

                snprintf(tempStr, INPUTLENGTH, "%s%d", args[i], getpid());
                args[i] = tempStr;
            
            }

        }
    }

    // check for exit 
    if (strcmp(args[0], "exit") == 0) {
        callExit();
    }

    // check for cd
    else if (strcmp(args[0], "cd") == 0) {
        callCD();
    }

    // check for status
    else if (strcmp(args[0], "status") == 0) {
        callStatus(childStatus);
    }
    // other commands
    else {
        callOtherCmds();

        // if termed with signal call status
        if (WIFSIGNALED(childStatus)) {
            callStatus(childStatus);
        }
    }
    return;
}


int main()
{

    // Handle CTRL-Z
    SIGTSTPAction.sa_handler = handle_SIGTSTP; 	// Direct SIGTSTP to the function handle_SIGTSTP()
    SIGTSTPAction.sa_flags = SA_RESTART; 		// Signals won't interrupt processes
    sigfillset(&SIGTSTPAction.sa_mask);			// Block all catchable signals
    sigaction(SIGTSTP, &SIGTSTPAction, NULL);	// Install signal handler

    // Handle CTRL-C
    SIGINTAction.sa_handler = SIG_IGN;			// Ignore initially
    sigfillset(&SIGINTAction.sa_mask); 			// Block all catchable signals
    sigaction(SIGINT, &SIGINTAction, NULL);		// Install signal handler


    // run the main loop
    while (1) {

        // reset global variables for next command
        memset(args, '\0', sizeof(args));
        num_args = 0;
        bgFlag = 0;
 
        fflush(stdin);
        fflush(stdout);

        getCommand();
        parseCommand();
    }

    return 0;
}
