#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

///////////////////////////////////////////////////////////////////////
//////////////////////////    Stack   ///////////////////////////
///////////////////////////////////////////////////////////////////////

struct stacknode
{
  pid_t value;
  struct stacknode* previous;
};

typedef struct stacknode Stacknode;

Stacknode* createStacknode(pid_t value){
  Stacknode* aux;
  aux = malloc(sizeof(Stacknode));
  aux->previous = NULL;
  aux->value = value;
  return aux;
}

void deleteStacknode(Stacknode* stacknode){
  free(stacknode);
}

struct stack
{
  Stacknode* last;
  int length;
};

typedef struct stack Stack;

void push(Stack* stack, pid_t nuevo){
  if (stack->length >= 1){
    Stacknode* aux = createStacknode(nuevo);
    aux->previous = stack->last;
    stack->last = aux;
    stack->length++;
  } else {
    Stacknode* aux = createStacknode(nuevo);
    stack->last = aux;
    stack->length++;
  }
}

pid_t pop(Stack* stack) {
  if (stack->length > 1) {
    Stacknode* aux = stack->last;
    stack->last = aux->previous;
    stack->length--;
    pid_t value = aux->value;
    deleteStacknode(aux);
    return value;
  } else if (stack->length == 1) {        // necesario ??? !!!
    Stacknode* aux = stack->last;
    stack->length--;
    pid_t value = aux->value;
    deleteStacknode(aux);
    return value;
  }
    else {
      return 0;
    }
}

Stack* createStack(){
  Stack* aux;
  aux = malloc(sizeof(Stack));
  aux->length = 0;
  return aux;
}

void deleteStack(Stack* stack){
  pid_t aux;
  while (stack->length){
    aux = pop(stack);
  }
  free(stack);
}

///////////////////////////////////////////////////////////////////////
//////////////////////////     END Stack    ///////////////////////////
///////////////////////////////////////////////////////////////////////


// global
int customPrompt = 0;
int customPath = 0;
char *_prompt = "~ ";
char *_path = "";
int _status = -1;
int _showStatus = -1;
pid_t mainPid;
pid_t _lastInBackground = -1;

Stack* process_stack;    // a este stack pusheas y pulleas.

void force_stop(int code) {
  if (getpid() == mainPid) { // only main process handle
    pid_t aux;
    aux = pop(process_stack);
    if ((_lastInBackground != -1) && (waitpid(aux, &_status, WNOHANG) == 0)) { // if there is a process running in background
      kill(aux, SIGKILL);
    }
    else {
      exit(1);
    }
  }
}

void setPath(char *newPath) {
  customPath = 1;
  _path = newPath;
  return;
}

void setPrompt(char *newPrompt) {
  char *getPos;
  int pos;
  customPrompt = 1;
  if (strstr(newPrompt, "*") != NULL) {
    getPos = strstr(newPrompt, "*");
    pos = getPos - newPrompt;
    _showStatus = pos;
  }
  else {
    _showStatus = -1;
  }
  _prompt = newPrompt;
  return;
}

char *getPrompt() {
  if (_showStatus == -1) {
    return _prompt;
  }
  else {
    char strStatus[12];
    sprintf(strStatus, "%d", _status);
    char *specialPrompt = malloc(strlen(strStatus) + strlen(_prompt)); // end character replaces '*'
    memcpy(specialPrompt, _prompt, _showStatus);
    memcpy(specialPrompt + _showStatus, strStatus, strlen(strStatus));
    memcpy(specialPrompt + _showStatus + strlen(strStatus), _prompt + _showStatus + 1 , strlen(_prompt) - _showStatus - 1);
    return specialPrompt;
  }
}

char **getArgs(char *command) {
  char delimiters[4];
  char *token;
  char **args = malloc(64 * sizeof(char *));
  int pos = 0;

  strcpy(delimiters, " \t\n");
  token = strtok(command, delimiters);

  while (token) {
    args[pos] = token;
    pos++;
    token = strtok(NULL, delimiters);
  }

  args[pos] = NULL;
  return args;
}

char *getCommand(void) {
  char *command = malloc(sizeof(char) * 1024);
  int pos;
  int c;

  for (pos = 0;1; pos++) {
    c = getchar();

    if (c == EOF || c == '\n') {
      command[pos] = '\0';
      return command;
    }
    else {
      command[pos] = c;
    }
  }
}

// asked Dave Marshall and edited John Difool, stackoverflow
int fileExists(char *fname) {
  FILE *file;
  if ((file = fopen(fname, "r"))) {
      fclose(file);
      return 1;
  }
  return 0;
}

// answered David Heffernan and edited unwind, stackoverflow
char* concat(const char *s1, const char *s2) {
  char *result = malloc(strlen(s1)+strlen(s2)+1);
  strcpy(result, s1);
  strcat(result, s2);
  return result;
}

char *lastArg(char **args, int kill) {
  int i;
  char *ret;
  for(i = 0; args[i]; i++)
  ;
  ret = args[--i];
  if (kill) args[i] = NULL;
  return ret;
}

int execArgsBackground(char **args) {
  pid_t pid;
  int j, n;
  char *amp;
  amp = "&";
  char *c;
  int k = 0;
  char **aux = malloc(64 * sizeof(char *));

  c = strtok(lastArg(args, 1), amp);
  if(!c) n = 1;
  else n = atoi(c);

  for (j = 0; j < n; j++) {
    pid = fork();
    if (pid == 0) {
      // customPath
      if (customPath && fileExists(concat(_path, args[0]))) {
        while (args[k] != NULL) {
          if (k == 0) {
            aux[k] = concat(_path, args[k]);
          }
          else {
            aux[k] = args[k];
          }
          k++;
        }
        if (execv(aux[0], aux) == -1) perror("msh");
        exit(1);
      }
      // no customPath
      if (execv(args[0], args) == -1) perror("msh");
      exit(1);
    }
    else if (pid > 0) {
      _lastInBackground = pid;
      push(process_stack, pid);
      waitpid(pid, &_status, WNOHANG);
    }
    else {
      printf("fork error\n");
      exit(1);
    }
  }
  return 1;
}

int execArgs(char **args) {
  int i;
  char *amp;
  amp = "&";
  pid_t pid;
  char **aux = malloc(64 * sizeof(char *));

  if (!args[0]) return 1;

  if (strcmp(args[0], "exit") == 0) {
    _Exit(1);
  }
  else if (strcmp(args[0], "setPrompt") == 0) {
    setPrompt(args[1]);
    return 1;
  }
  else if (strcmp(args[0], "setPath") == 0) {
    setPath(args[1]);
    return 1;
  }

  if (strstr(lastArg(args, 0), amp) != NULL) {
    return execArgsBackground(args);
  }

  pid = fork();

  if (pid == 0) {
    if (customPath && fileExists(concat(_path, args[0]))) {
      for (i = 0; args[i]; i++) {
        if (i == 0) {
          aux[i] = concat(_path, args[i]);
        }
        else {
          aux[i] = args[i];
        }
      }

      if (execv(aux[0], aux) == -1) perror("msh");
      exit(1);
    }

    if (execv(args[0], args) == -1) perror("msh");
    exit(1);
  }
  else if (pid > 0) {
    waitpid(pid, &_status, WUNTRACED);
  }
  return 1;
}

void now(void) {
  time_t now;
  time(&now);
  struct tm *local;
  local = localtime(&now);
  printf("%02d:%02d:%02d ", local->tm_hour, local->tm_min, local->tm_sec);
  return;
}

int main(int argc, char const *argv[]) {
  char **args;
  char *command;

   process_stack = createStack();

  mainPid = getpid();
  signal(SIGINT, force_stop);

  while (1) {
    if (_lastInBackground != -1) waitpid(_lastInBackground, &_status, WNOHANG);
    if (customPath) printf("(%s)\n", _path);
    if (!customPrompt) now();
    printf("%s ", getPrompt());
    command = getCommand();
    args = getArgs(command);
    execArgs(args);
  }
  return 0;
}
