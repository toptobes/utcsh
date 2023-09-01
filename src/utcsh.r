#ifndef UTCSH_UTCSH_R
#define UTCSH_UTCSH_R

#include <stdlib.h>

typedef struct Command {
  int8_t argc;
  char **argv;
  char *outputFile;
} Command;

typedef void (*CommandFnImpl)();

typedef struct CommandFn {
  const char *name;
  CommandFnImpl execute;
} CommandFn;

Command *read_commands(int *ncmds);
Command *parse_commands(char *cmdline, int *ncommands);
void parse_command(char *segment, Command *cmd);

void eval(struct Command *cmd, int ncmds);

#define BUILTINS \
  X(exit)        \
  X(cd)          \
  X(path)        \

#define X(name) void name##_fn(Command *cmd);
  BUILTINS
  X(extrnl)
#undef X

extern CommandFn functions[];

#endif// UTCSH_UTCSH_R
