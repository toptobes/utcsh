#include "util.h"
#include "utcsh.r"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/wait.h>

// The array for holding shell paths. Can be edited by the functions in util.c
char shell_paths[MAX_ENTRIES_IN_SHELLPATH][MAX_CHARS_PER_CMDLINE];
static char prompt[] = "utcsh> ";
static char *default_shell_path[2] = { "/bin", NULL };

CommandFn functions[] = {
  #define X(name) {#name, name##_fn},
    BUILTINS
    { NULL, extrnl_fn },
  #undef X
};

int main(int argc, char **argv)
{
  autoclose int fd = -1;

  if (argc == 2) {
    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
      ppanic("open");
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
      ppanic("dup2");
    }
  }

  set_shell_path(default_shell_path);

  do {
    if (fd == -1) {
      printf("%s", prompt);
    }

    let ncmds = 0;
    let cmds = read_commands(&ncmds);

    printcmds(cmds, ncmds);

    eval(cmds, ncmds);
  } while(fd == -1);

  return 0;
}

Command *read_commands(int *ncmds) {
  size_t n = 0;
  autofree char *buffer = NULL;

  let nread = getline(&buffer, &n, stdin);

  if (nread == -1) {
    exit_fn(0);
  }

  if (buffer[nread - 1] == '\n') {
    buffer[nread - 1] = '\0';
  }

  return parse_commands(buffer, ncmds);
}

Command *parse_commands(char *cmdline, int *ncmds) 
{
  char *saveptr1;

  *ncmds = 1;

  for (size_t i = 0; i < strlen(cmdline); i++)
  {
    *ncmds += (cmdline[i] == '&');
  }

  Command *commands = calloc(*ncmds, sizeof(Command));

  char *segment = strtok_r(cmdline, "&", &saveptr1);

  for (int i = 0; i < *ncmds; i++) 
  {
    parse_command(strdup(segment), commands + i);
    segment = strtok_r(NULL, "&", &saveptr1);
  }

  return commands;
}

void parse_command(char *segment, Command *cmd)
{
  char *token = strtok(segment, " ");
  let nargs = 0;

  while (token)
  {
    if (strcmp(token, ">") == 0 ) 
    {
      token = strtok(NULL, " ");
      cmd->outputFile = strdup(token);
    } 
    else 
    {
      cmd->argv = realloc(cmd->argv, (nargs + 1) * sizeof(char*));
      cmd->argv[nargs] = strdup(token);
      nargs++;
    }
    token = strtok(NULL, " ");
  }

  cmd->argv = realloc(cmd->argv, (nargs + 1) * sizeof(char*));
  cmd->argv[nargs] = NULL;

  cmd->argc = nargs - 1;
}

static void exec_single(Command *cmd);

void eval(Command *cmd, int ncmds)
{
  pid_t pids[ncmds - 1];

  for (int i = 0; i < ncmds; i++)
  {
    let doInBackground = i < (ncmds - 1);

    if (doInBackground) {
      pids[i] = fork();

      if (pids[i] == CHILD_PROCESS)
      {
        return exec_single(cmd + i);
      }
    }
    else
    {
      exec_single(cmd + i);
    }
  }

  for (int i = 0; i < ncmds - 1; i++)
  {
    waitpid(pids[i], NULL, 0);
  }
}

static void exec_single(Command *cmd)
{
  for (CommandFn *fn = functions; true; fn++)
  {
    if (fn->name == NULL || strcmp(*cmd->argv, fn->name) == 0)
    {
      return fn->execute(cmd);
    }
  }
}

void exit_fn(Command *cmd)
{
  if (cmd->argc != 0) {
    return scold_user("'exit' doesn't take any arguments\n");
  }

  exit(0);
}

void cd_fn(Command *cmd) 
{
  if (cmd->argc != 1) {
    return scold_user("'cd' requires one argument\n");
  }

  __attribute__((unused)) int ret = chdir(cmd->argv[1]);
}

void path_fn(Command *cmd)
{
  (void)cmd;
}

void extrnl_fn(Command *cmd)
{
  let pid = fork();

  if (pid == CHILD_PROCESS)
  {
    execv(cmd->argv[0], cmd->argv + 1);
    perror("execv");
  }
  else
  {
    waitpid(pid, NULL, 0);
  }
}
