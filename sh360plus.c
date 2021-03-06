/*
 * Several ideas and code snippets from sh360.c
 * are taken from Dr.Zastre CSC360, summer 2018 course.
 */

/*
 * In addition to the basic functionality required by sh360, sh360plus adds 2
 * new features:
 *
 * 1. The combination of output redirection and piping into a command ORPP, the
 * pipe limit of 3 still remains.
 * 2. A history feature that keeps track of the previous 10 commands and allows
 * users to execute the command given the command id, similar to normal bash. ie
 * if they type ls, it will be stored in history and then they can execute `!<ls
 * command id>` to execute ls again.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_INPUT_LINE 80
#define RC_FILE_NAME ".sh360rc"
#define MAX_NUM_TOKENS 25  // 3 commands * 7 args = 21 + 3 extra
#define MAX_PROMPT 10
#define MAX_ARGS 7
#define MAX_PIPES 2
#define MAX_PATH_LENGTH 20
#define MAX_DIRECTORIES 10
#define MAX_HISTORY 10

// Determines whether a file exists or not
int fileExists(char* fileName) {
  struct stat buffer;
  return (stat(fileName, &buffer) == 0);
}

// The main logic for iterating through the RC file to check paths
int getFile(char* fileName) {
  char path[MAX_PATH_LENGTH + MAX_INPUT_LINE + 1] = "";
  FILE* file = fopen(RC_FILE_NAME, "r");
  int i;
  int lineCounter = 0;
  while (fgets(path, MAX_PATH_LENGTH, file) != NULL) {
    lineCounter++;
    if (lineCounter == 1) {
      continue;
    }
    if (lineCounter > MAX_DIRECTORIES) {
      fclose(file);
      return 0;
    }
    if (path[strlen(path) - 2] == '\r') {
      path[strlen(path) - 2] = '\0';
    } else if (path[strlen(path) - 1] == '\n') {
      path[strlen(path) - 1] = '\0';
    } else {
      fprintf(stderr, "Path directories must be followed by a new line");
    }
    if (path[strlen(path) - 1] != '/') {
      strcat(path, "/");
    }
    strcat(path, fileName);

    if (fileExists(path)) {
      strcpy(fileName, path);
      fclose(file);
      return 1;
    }
  }
  fclose(file);
  return 0;
}

// A Wrapper function around checking if the file exists or not
// Also updates filename to include path, ie `ls`->`/bin/ls`
int verifyFile(char* fileName, char* input) {
  char command[MAX_PROMPT];
  strcpy(command, fileName);
  if (getFile(command)) {
    strcpy(fileName, command);
    return 1;
  } else {
    fprintf(stderr, "%s: command not found\n", input);
    return 0;
  }
}

// Find the index of the symbol that was passed in
int findIndex(char* symbol, char* token[]) {
  int arrowIndex = 0;
  while (token[arrowIndex] != NULL) {
    if (strcmp(token[arrowIndex], symbol) == 0) {
      return arrowIndex;
    }
    arrowIndex++;
  }
  return -1;
}

// Main function for redirecting output to file(OR)
void executeAndSendToFile(char* token[], char* input, char* envp[]) {
  int fd, pid, status;
  int arrowIndex = findIndex("->", token);
  if (arrowIndex == -1) {
    fprintf(stderr, "command %s must contain ->\n", input);
    return;
  }
  if (getLength(token) != arrowIndex + 2 ||
      strcmp(token[arrowIndex + 1], "->") == 0) {
    fprintf(stderr,
            "command %s must contain one output file following the ->\n",
            input);
    return;
  }

  fd = open(token[arrowIndex + 1], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    fprintf(stderr, "cannot open %s for writing\n", token[arrowIndex + 1]);
    return;
  }
  dup2(fd, 1);
  char* newToken[MAX_NUM_TOKENS + 1];
  int i;
  for (i = 1; i < arrowIndex; i++) {
    newToken[i - 1] = token[i];
  }
  newToken[i - 1] = 0;
  if (getLength(newToken) > MAX_ARGS + 1) {
    fprintf(stderr, "Too many arguments, max of %d is allowed\n", MAX_ARGS);
    return;
  }
  execve(newToken[0], newToken, envp);
}

// Main function for piping(PP)
void executeAndPipe(char* token[], char* input, char* envp[], int output) {
  int pid, status, i;
  int fd[2];
  int arrowIndexes[MAX_PROMPT];
  char* tokens[MAX_PIPES + 1][MAX_NUM_TOKENS + 1];
  arrowIndexes[0] = -1;
  int j = 0;
  int arrowCounter = 0;
  int separateCommand = 0;
  int commandLength = 0;
  char command[MAX_ARGS + 2];
  for (i = 1; i < getLength(token); i++) {
    if (strcmp(token[i], "->") == 0) {
      tokens[separateCommand][commandLength] = 0;
      arrowCounter++;
      separateCommand++;
      if (separateCommand > MAX_PIPES) {
        fprintf(stderr, "Too many pipes, max of %d is allowed\n", MAX_PIPES);
        return;
      }
      commandLength = 0;
      if (i == getLength(token) - 1 || strcmp(token[i + 1], "->") == 0) {
        if (output == 1) {
          fprintf(stderr,
                  "command %s must contain an output file following ->\n",
                  input);
        } else {
          fprintf(stderr,
                  "command %s must contain another command following ->\n",
                  input);
        }
        return;
      } else {
        if (i + 2 == getLength(token)) {
          if (output == 1) {
            tokens[separateCommand][commandLength] = strdup(token[i + 1]);
            commandLength++;
            break;
          }
        }
        strcpy(command, token[i + 1]);
        if (!verifyFile(command, input)) {
          return;
        }
        token[i + 1] = command;
      }
    } else {
      tokens[separateCommand][commandLength] = strdup(token[i]);
      commandLength++;
      if (commandLength > MAX_ARGS + 1) {
        printf("%d\n", commandLength);
        fprintf(stderr, "Too many arguments, max of %d is allowed\n", MAX_ARGS);
        return;
      }
    }
  }
  tokens[separateCommand][commandLength] = 0;
  if (arrowCounter == 0) {
    fprintf(stderr, "command %s must contain ->\n", input);
    return;
  }

  int fdBackup = 0;

  for (i = 0; i <= arrowCounter; i++) {
    pipe(fd);
    if ((pid = fork()) == 0) {
      dup2(fdBackup, 0);
      if (i != arrowCounter) {
        if (i + 1 == arrowCounter && output == 1) {
          fflush(stdout);
          if (getLength(tokens) != i + 1) {
            fprintf(stderr,
                    "command %s must contain an output file following ->\n",
                    input);
          }
          int fd = open(tokens[i + 1][0], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
          if (fd == -1) {
            fprintf(stderr, "cannot open %s for writing\n", tokens[i][0]);
            return;
          }
          dup2(fd, 1);
          execve(tokens[i][0], tokens[i], envp);
          // Shouldn't reach this
          exit(0);
        } else {
          dup2(fd[1], 1);
        }
      }
      close(fd[0]);
      execve(tokens[i][0], tokens[i], envp);
      // Shouldn't reach this
      exit(0);
    }
    waitpid(pid, &status, 0);
    close(fd[1]);
    fdBackup = fd[0];
  }
}

// Given string, tokenize it so that spaces are separated into different
// elements
void tokenize(char* token[], char* input) {
  int num_tokens = 0;
  for (token[num_tokens] = strtok(input, " "); token[num_tokens] != NULL;
       token[num_tokens] = strtok(NULL, " ")) {
    num_tokens++;
  }
  num_tokens++;
  token[num_tokens] = 0;
}

// Helper function for getting length of string array
int getLength(char* token[]) {
  int i = 0;
  while (token[i] != 0) {
    i++;
  }
  return i;
}

// Used to print the history
void printHistory(char* history[]) {
  int i;
  for (i = 0; history[i] != NULL; i++) {
    if (strcmp(history[i], "") == 0) {
      continue;
    }
    printf("%d %s\n", i, history[i]);
  }
}

// Called by the main function, this is the main loop for redirecting to
// different function
void startShell() {
  char input[MAX_INPUT_LINE];
  char fullPath[MAX_INPUT_LINE + MAX_PATH_LENGTH];
  char* token[MAX_NUM_TOKENS];
  char* history[MAX_HISTORY] = {""};
  int pid;
  char* envp[] = {0};
  int status;
  int historyCounter = 0;

  char prompt[MAX_PROMPT];
  FILE* file = fopen(RC_FILE_NAME, "r");
  if (!file) {
    fprintf(stderr, "Error: File open error!\n");
    return;
  }
  fscanf(file, "%s", prompt);
  fclose(file);
  if (strlen(prompt) > MAX_PROMPT) {
    fprintf(stderr,
            "Error: Prompt too long, must be less than 10 characters\n");
    return;
  }

  for (;;) {
    printf("%s ", prompt);
    fflush(stdout);
    fgets(input, MAX_INPUT_LINE, stdin);
    if (input[strlen(input) - 1] == '\n') {
      input[strlen(input) - 1] = '\0';
    }
    if (strcmp(input, "history") == 0) {
      printHistory(history);
      continue;
    }
    if (strcmp(input, "exit") == 0) {
      return;
    }
    if (strcmp(input, "") == 0) {
      continue;
    }
    if (input[0] == '!') {
      if (getLength(history) < (input[1] - '0') || (input[1] - '0') < 0) {
        fprintf(stderr, "%d: event not found\n", (input[1] - '0'));
        continue;
      }
      strcpy(input, history[input[1] - '0']);
    } else {
      history[historyCounter] = strdup(input);
      historyCounter = (historyCounter + 1) % MAX_HISTORY;
    }
    tokenize(token, input);
    char command[MAX_PROMPT];
    if ((pid = fork()) == 0) {
      if ((strcmp(token[0], "OR") == 0) || (strcmp(token[0], "PP") == 0) ||
          (strcmp(token[0], "ORPP") == 0)) {
        if (getLength(token) == 1) {
          fprintf(stderr,
                  "%s: command not found, must have tokens before and after "
                  "arrows\n",
                  input);
          continue;
        }
        strcpy(command, token[1]);
        if (!verifyFile(command, input)) {
          continue;
        }

        token[1] = command;
        if ((strcmp(token[0], "OR") == 0)) {
          executeAndSendToFile(token, input, envp);
        } else if ((strcmp(token[0], "PP") == 0)) {
          executeAndPipe(token, input, envp, 0);
        } else if ((strcmp(token[0], "ORPP") == 0)) {
          executeAndPipe(token, input, envp, 1);
        }

      } else {
        strcpy(command, token[0]);
        if (getLength(token) > MAX_ARGS + 1) {
          fprintf(stderr, "Too many arguments, max of %d is allowed\n",
                  MAX_ARGS);
          exit(0);
        }
        if (verifyFile(command, input)) {
          token[0] = command;
          execve(token[0], token, envp);
        }
      }
      exit(0);
    }
    waitpid(pid, &status, 0);
  }
}

int main(int argc, char* argv[]) {
  startShell();
  exit(0);
}
