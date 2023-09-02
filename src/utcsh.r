#ifndef UTCSH_UTCSH_R
#define UTCSH_UTCSH_R

#include <stdlib.h>

typedef struct Command {
  int8_t argc;
  char **argv;
  char *outputFile;
} Command;

typedef void (*CommandFnImpl)(Command *cmd);

typedef struct CommandFn {
  const char *name;
  CommandFnImpl execute;
} CommandFn;

int set_input_source(int argc, char **argv);

Command *read_commands(int *ncmds);
Command *parse_commands(char *cmdline, int *ncommands);
bool parse_command(char *segment, Command *cmd);

int redirectStdout(const char *outputFile);
void unredirectStdout();

void eval(struct Command *cmd, int ncmds);
void exec_single(Command *cmd);

void destruct(Command *cmds, int ncmds);

#define BUILTINS_X \
  X(exit)          \
  X(cd)            \
  X(path)          \
  X(toggledebug)   \

#define X(name) void name##_builtin(Command *cmd);
  BUILTINS_X
  X(external)
#undef X

extern CommandFn functions[];

#endif//UTCSH_UTCSH_R
