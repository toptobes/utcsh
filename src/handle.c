#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

void handler(__attribute__((unused)) int signal)
{
  write(STDOUT_FILENO, "Exiting\n", 8);
  exit(1);
}

int main(void)
{
  sigaction(SIGUSR1, &(struct sigaction) {
    .sa_handler = &handler,
  }, NULL);

  struct timespec ts;
  ts.tv_sec = 1;

  printf("%d\n", getpid());

  while (1)
  {
    nanosleep(&ts, NULL);
    puts("Still here");
  }
}
