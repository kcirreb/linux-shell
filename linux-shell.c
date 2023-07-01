#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>

// if child process is ready to execute
int ready = 0;
// if there is any process running
int running = 0;
// number of commands
int n = 0;

// handler for SIGINT signal
void sigIntHandler(int signum) {
  // print shell prompt if no process running
  if (!running) {
    printf("\n$$ linux-shell ## ");
    fflush(stdout);
  }
}

// handler for SIGUSR1 signal
void sigUsr1Handler(int signum) {
  // allow child process to execute
  ready = 1;
}

// read and return user input
char *readInput() {
  size_t size = 1024; // max 1024 characters
  char *input = malloc(size * sizeof(char));

  ssize_t length = getline(&input, &size, stdin);

  // remove trailing '\n' from getline()
  if (input[length - 1] == '\n') input[length - 1] = '\0';
  // remove leading and trailing ' '
  while(isspace(*input)) input++;
  char *end = input + strlen(input) - 1;
  while(end > input && isspace(*end)) end--;
  end[1] = '\0';

  return input;
}

// parse user input and return array of commands
char **parseInput(char *input) {
  size_t size = 30; // max 30 strings
  char **commands = malloc(size * sizeof(char *));
  int index = 0;

  // break input using '|' as delimiter
  char *token = strtok(input, "|");
  while(token) {
    // remove leading and trailing ' '
    while(isspace(*token)) token++;
    char *end = token + strlen(token) - 1;
    while(end > token && isspace(*end)) end--;
    end[1] = '\0';

    // store non-empty commands into array
    if (strlen(token) > 0) {
      commands[index] = token;
      index++;
    }
    token = strtok(NULL, "|");
  }

  // set number of commands
  n = index;

  return commands;
}

// parse command and return arrary of program name and arguments
char **parseCommand(char *input) {
  size_t size = 30; // max 30 strings
  char **argv = malloc(size * sizeof(char *));
  int index = 0;

  // break command using delimiters
  char *delimiters = " \t\r\n";
  char *token = strtok(input, delimiters);
  while(token) {
    // store program name and arguments into array
    argv[index] = token;
    index++;
    token = strtok(NULL, " ");
  }

  // set last argument as NULL for execvp()
  argv[index] = NULL;

  return argv;
}

// create pipes and execute commands
void pipeCommands(char **commands, int n, int timeX) {
  // reset ready such that child process is unable to execute
  ready = 0;

  // arrays for pid, status, used for each child process
  int pidArray[5];
  int statusArray[5];
  struct rusage usedArray[5];
  
  // array of pipes
	int pfd[4][2];

  // create all required pipes
  for (int i = 0; i < n - 1; i++) {
    pipe(pfd[i]);
  }

  // loop through all commands
  for (int i = 0; i < n; i++) {
    // create child process
    pid_t pid = fork();
    pidArray[i] = pid;

    // error in creating child process
    if (pid < 0) {
      printf("linux-shell: fork: %s\n", strerror(errno));
      exit(-1);
    } 
    // child process
    else if (pid == 0) {
      // if not first command, set previous pipe's read end to stdin
      if (i != 0) {
        dup2(pfd[i-1][0], 0);
      }
      // if not last command, set current pipe's write end to stdout
      if (i != n - 1) {
        dup2(pfd[i][1], 1);
      }

      // close all pipes
      for (int j = 0; j < n - 1; j++) {
        close(pfd[j][0]);
        close(pfd[j][1]);
      }

      // wait until parent process sends SIGUSR1 signal
      while (!ready) {
        usleep(100);
      }

      // parse current command into program name and arguments
      char **argv = parseCommand(commands[i]);

      // execute program
      if (execvp(argv[0], argv) == -1) {
        printf("linux-shell: '%s': %s\n", argv[0], strerror(errno));
        exit(-1);
      }
    } 
    // parent process
    else {
      // send SIGUSR1 signal to allow child process to execute
      kill(pid, SIGUSR1);
    }
  }

  // close all pipes
  for (int i = 0; i < n - 1; i++) {
    close(pfd[i][0]);
    close(pfd[i][1]);
  }

  // loop through all child processes
  for (int i = 0; i < n; i++) {
    // parent process waits for child processes to terminate
    wait4(pidArray[i], &statusArray[i], 0, &usedArray[i]);

    // print signal info if child process is terminated by signal
    if (WIFSIGNALED(statusArray[i])) {
      printf("%s\n", strsignal(WTERMSIG(statusArray[i])));
    }
    
    // print statistics of terminated child process if timeX is set
    if (timeX) {
      int pid = pidArray[i]; // process ID
      char *cmd = parseCommand(commands[i])[0]; // program name
      double user = usedArray[i].ru_utime.tv_sec + usedArray[i].ru_utime.tv_usec / 1000000.0; // user cpu time (in seconds)
      double sys = usedArray[i].ru_stime.tv_sec + usedArray[i].ru_stime.tv_usec / 1000000.0; // system cpu time (in seconds)

      printf("(PID)%d  (CMD)%s    (user)%.3f s  (sys)%.3f s\n", pid, cmd, user, sys);
    }
  }
}

int main() {
  // set handler for SIGINT and SIGUSR1 signals
  signal(SIGINT, sigIntHandler);
  signal(SIGUSR1, sigUsr1Handler);

  // loop until exit shell
  while(1) {
    // unset timeX command
    int timeX = 0;

    // pint shell prompt
    printf("$$ linux-shell ## ");

    // read user input
    char *input = readInput();
    
    // check for empty input
    if (input[0] == '\0') {
      continue;
    }

    // check for exit command
    if (strncmp(input, "exit", 4) == 0) {
      // remove "exit" from input
      input += 4;
      // remove leading ' '
      while(isspace(*input)) input++;
      // exit command with other arguments
      if (input[0] != '\0') {
				printf("linux-shell: 'exit' with other arguments!!!\n");
        continue;
      } 
      // exit shell
      else {
        printf("linux-shell: Terminated\n");
				exit(0);
      }
    }

    // check for timeX command
    if (strncmp(input, "timeX", 5) == 0) {
      // remove "timeX" from input
      input += 5;
      // remove leading ' '
      while(isspace(*input)) input++;
      // timeX command without another command
      if (input[0] == '\0') {
				printf("linux-shell: 'timeX' cannot be a standalone command\n");
        continue;
      } 
      // set timeX command
      else {
        timeX = 1;
      }
    }
    
    // check for pipe error
    int pipes = 0;
    for (int i = 0; i < strlen(input); i++) {
      if (input[i] == '|') pipes++;
    }
    if (input[0] == '|') {
      printf("linux-shell: should not have | as the first character\n");
      continue;
    }
    if (input[strlen(input) - 1] == '|') {
      printf("linux-shell: should not have | as the last character\n");
      continue;
    }

    // parse user input
    char **commands = parseInput(input);

    // check for pipe error
    if (pipes >= n) {
      printf("linux-shell: should not have two consecutive | without in-between command\n");
      free(commands);
      continue;
    }

    // create pipes and execute commands
    running = 1;
    pipeCommands(commands, n, timeX);
    running = 0;

    // reset number of commands
    n = 0;

    // release resources
    free(commands);
  }

  return 0;
}