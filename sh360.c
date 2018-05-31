/*
 * Several ideas and code snippets from sh360.c
 * are taken from Dr.Zastre CSC360, summer 2018 course.
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
#define MAX_PIPES 3
#define MAX_PATH_LENGTH 20
#define MAX_DIRECTORIES 10

int fileExists(char* filename) {
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

int getFile(char* fileName) {
  char fullPathBuff[MAX_PATH_LENGTH + MAX_INPUT_LINE + 1];
  FILE* file = fopen(RC_FILE_NAME, "r");
  int i;
  int lineCounter = 0;
  while (fgets(fullPathBuff, MAX_PATH_LENGTH - 1, file)) {
    lineCounter++;
    if (lineCounter == 1) {
      continue;
    }
    if (lineCounter > MAX_DIRECTORIES) {
      fclose(file);
      return 0;
    }
    if (fullPathBuff[strlen(fullPathBuff) - 1] == '\n') {
      fullPathBuff[strlen(fullPathBuff) - 1] = '\0';
    }
    int j = 0;
    for (i = strlen(fullPathBuff) - 1;
         i < (int)(strlen(fullPathBuff) + strlen(fileName)); i++) {
      fullPathBuff[i] = fileName[j];
      j++;
    }
    if (fileExists(fullPathBuff)) {
      strcpy(fileName, fullPathBuff);
      fclose(file);
      return 1;
    }
  }
  fclose(file);
  return 0;
}

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

int findIndex(char* symbol, char* token[]) {
  int arrowIndex = 0;
  while (token[arrowIndex] != NULL) {
    if (strcmp(token[arrowIndex], "->") == 0) {
      return arrowIndex;
    }
    arrowIndex++;
  }
  return -1;
}

void executeAndSendToFile(char* token[], char* input, char* envp[]) {
  int fd, pid, status;
  int arrowIndex = findIndex("->", token);
  if (arrowIndex == -1) {
    fprintf(stderr, "command %s must contain ->\n", input);
    return;
  }
  if (getLength(token) != arrowIndex + 2) {
    fprintf(stderr, "command %s must contain an output file following the ->\n",
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

void executeAndPipe(char* token[], char* input, char* envp[]) {
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
      commandLength = 0;
      if (i == getLength(token) - 1 || strcmp(token[i + 1], "->") == 0) {
        fprintf(stderr,
                "command %s must contain another command following ->\n",
                input);
        return;
      } else {
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
        dup2(fd[1], 1);
      }
      close(fd[0]);
      execve(tokens[i][0], tokens[i], envp);
      exit(0);
    }
    waitpid(pid, &status, 0);
    close(fd[1]);
    fdBackup = fd[0];
  }
}

void tokenize(char* token[], char* input) {
  int num_tokens = 0;
  for (token[num_tokens] = strtok(input, " "); token[num_tokens] != NULL;
       token[num_tokens] = strtok(NULL, " ")) {
    num_tokens++;
  }
  num_tokens++;
  token[num_tokens] = 0;
}

int getLength(char* token[]) {
  int i = 0;
  while (token[i] != 0) {
    i++;
  }
  return i;
}

void startShell() {
  char input[MAX_INPUT_LINE];
  char fullPath[MAX_INPUT_LINE + MAX_PATH_LENGTH];
  char* token[MAX_NUM_TOKENS];
  int pid;
  char* envp[] = {0};
  int status;

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
    if (strcmp(input, "exit") == 0) {
      return;
    }
    if (strcmp(input, "") == 0) {
      continue;
    }
    tokenize(token, input);
    char command[MAX_PROMPT];
    if ((pid = fork()) == 0) {
      if ((strcmp(token[0], "OR") == 0) || (strcmp(token[0], "PP") == 0)) {
        if (getLength(token) == 1) {
          fprintf(stderr, "%s: command not found\n", input);
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
          executeAndPipe(token, input, envp);
        }

      } else {
        strcpy(command, token[0]);
        if (getLength(token) > MAX_ARGS + 1) {
          fprintf(stderr, "Too many arguments, max of %d is allowed\n",
                  MAX_ARGS);
          continue;
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
