#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "util.h"

const int MAX = 13;

static void doFib(int n, int doPrint);

inline static void unix_error(char *msg)
{
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

int main(int argc, char **argv)
{
  int arg;
  int print = 1;

  if (argc != 2)
  {
    fprintf(stderr, "Usage: fib <num>\n");
    exit(-1);
  }

  arg = atoi(argv[1]);
  if (arg < 0 || arg > MAX)
  {
    fprintf(stderr, "number must be between 0 and %d\n", MAX);
    exit(-1);
  }

  doFib(arg, print);

  return 0;
}

static int fib(int n)
{
  if (n <= 1)
  {
    return n;
  }

  int status1, status2;

  if (fork() == CHILD_PROCESS)
  {
    exit(fib(n - 1));
  }

  if (fork() == CHILD_PROCESS)
  {
    exit(fib(n - 2));
  }

  wait(&status1);
  wait(&status2);

  return WEXITSTATUS(status1) + WEXITSTATUS(status2);
}

static void doFib(int n, __attribute__((unused)) int doPrint)
{
  printf("%d\n", fib(n));
}
