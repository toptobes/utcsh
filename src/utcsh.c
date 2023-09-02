#include "util.h"
#include "utcsh.r"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/wait.h>

char shell_paths[MAX_ENTRIES_IN_SHELLPATH][MAX_CHARS_PER_CMDLINE];
static char prompt[] = "utcsh> ";
static char *default_shell_path[2] = { "/bin", NULL };

CommandFn functions[] = {
  #define X(name) { #name, name##_builtin },
    BUILTINS_X
    { NULL, external_builtin },
  #undef X
};

int main(int argc, char **argv)
{
  autoclose int fd = set_input_source(argc, argv);
  set_shell_path(default_shell_path);

  do {
    if (fd == -1) {
      printf("%s", prompt);
    }

    let ncmds = 0;
    let cmds = read_commands(&ncmds);

    if (!cmds) {
      continue;
    }

    printcmds(cmds, ncmds);

    eval(cmds, ncmds);

    destruct(cmds, ncmds);
  } while(fd == -1);

  return 0;
}

int set_input_source(int argc, char **argv)
{
  let fd = -1;

  if (argc == 2)
  {
    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
      ppanic("open");
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
      ppanic("dup2");
    }
  }

  return fd;
}

Command *read_commands(int *ncmds) 
{
  size_t n = 0;
  autofree char *buffer = NULL;

  let nread = getline(&buffer, &n, stdin);

  if (nread == -1) {
    exit_builtin(0);
  }

  if (buffer[nread - 1] == '\n') {
    buffer[nread - 1] = '\0';
  }

  return parse_commands(buffer, ncmds);
}

Command *parse_commands(char *cmdline, int *ncmds) 
{
  char *cmdtokstate;

  *ncmds = 1;
  for (size_t i = 0; i < strlen(cmdline); i++)
  {
    *ncmds += (cmdline[i] == '&');
  }

  Command *commands = calloc(*ncmds, sizeof(Command));
  char *cmdtxt = strtok_r(cmdline, "&", &cmdtokstate);

  if (cmdtxt == NULL) {
    return NULL;
  }

  for (let i = 0; i < *ncmds; i++) 
  {
    if (!parse_command(cmdtxt, commands + i)) {
      return NULL;
    }
    cmdtxt = strtok_r(NULL, "&", &cmdtokstate);
  }

  return commands;
}

bool parse_command(char *cmdtxt, Command *cmd)
{
  let token = strtok(cmdtxt, " ");

  let nargs = 0;
  let hasRedirect = false;

  while (token)
  {
    if (strcmp(token, ">") == 0)
    {
      if (hasRedirect)
      {
        scold_user("Single command can't have multiple redirects smh");
        return false;
      }

      token = strtok(NULL, " ");
      cmd->outputFile = strdup(token);
      hasRedirect = true;
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
  return true;
}

void eval(Command *cmd, int ncmds)
{
  pid_t pids[(ncmds - 1) >= 1 ? (ncmds - 1) : 1];

  for (let i = 0; i < ncmds; i++)
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

  for (let i = 0; i < ncmds - 1; i++)
  {
    waitpid(pids[i], NULL, 0);
  }
}

int original_stdout;

__attribute__((constructor))
void initialize() {
  original_stdout = dup(STDOUT_FILENO);

  if (original_stdout == -1) {
    ppanic("Failed to duplicate stdout");
  }
}

void exec_single(Command *cmd)
{
  for (CommandFn *fn = functions; true; fn++)
  {
    if (fn->name == NULL || strcmp(*cmd->argv, fn->name) == 0)
    {
      autoclose int fd = -1;

      if (cmd->outputFile != NULL) {
        fd = redirectStdout(cmd->outputFile);

        if (fd == -1) {
          scold_user("Error writing to file '%s'", cmd->outputFile);
          return;
        }
      }

      fn->execute(cmd);

      if (fd != -1) {
        unredirectStdout();
      }

      break;
    }
  }
}

int redirectStdout(const char *outputFile) 
{
    let fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
      ppanic("Failed to open file");
    }
    dup2(fd, STDOUT_FILENO);
    return fd;
}

void unredirectStdout()
{
    dup2(original_stdout, STDOUT_FILENO);
}

void destruct(Command *cmds, int ncmds)
{
  for (let i = 0; i < ncmds; i++)
  {
    let cmd = cmds + i;

    for (let j = 0; j < cmd->argc + 1; j++)
    {
      free(cmd->argv[j]);
    }

    free(cmd->argv);
    free(cmd->outputFile);
  }

  free(cmds);
}

void exit_builtin(Command *cmd)
{
  if (cmd->argc != 0) {
    return scold_user("'exit' doesn't take any arguments");
  }

  exit(0);
}

void cd_builtin(Command *cmd) 
{
  if (cmd->argc != 1) {
    return scold_user("'cd' requires one argument");
  }

  let ret = chdir(cmd->argv[1]);

  if (ret == -1) {
    scold_user("failed to change directories");
  }
}

void path_builtin(Command *cmd)
{
  set_shell_path(cmd->argv + 1);
}

void toggledebug_builtin(unused Command *cmd)
{
  in_debug_mode = !in_debug_mode;
}

void external_builtin(Command *cmd)
{
  let pid = fork();

  if (pid == CHILD_PROCESS)
  {
    char *path = (is_absolute_path(cmd->argv[0]))
      ? cmd->argv[0]
      : find_exe(cmd->argv[0]);

    if (path == NULL) 
    {
      scold_user("Could not find executable '%s'", cmd->argv[0]);
      return;
    }

    cmd->argv[0] = path;
    execv(cmd->argv[0], cmd->argv);
    perror("execv");
  }
  else
  {
    waitpid(pid, NULL, 0);
  }
}
