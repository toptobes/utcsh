/** A home for the non-allocation related functions that we want to fail. */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

#include "debug.h"

// Defined within alloc.c
extern _Atomic bool withinEBB;

enum FalliableFunc {
  Open,
  Close,
  Fopen,
  Fclose,
  Fseek,
  Creat,
  Dup2,
  Getcwd,
  Getline,
  Execv,
  Fork,
  Wait
};

typedef int (*open_ty)(const char *pathname, int flags);
typedef int (*close_ty)(int fd);
typedef FILE *(*fopen_ty)(const char *restrict pathname,
                          const char *restrict mode);
typedef int (*fclose_ty)(FILE *stream);
typedef int (*fseek_ty)(FILE *stream, long offset, int whence);
typedef int (*creat_ty)(const char *path, mode_t mode);
typedef int (*dup2_ty)(int filedes, int filedes2);
typedef char *(*getcwd_ty)(char *buf, size_t size);
typedef char (*getwd_ty)(char *buf); // Should always fail
typedef ssize_t (*getline_ty)(char **restrict lineptr, size_t *restrict n,
                              FILE *restrict stream);
typedef int (*execv_ty)(const char *pathname, char *const argv[]);
typedef pid_t (*fork_ty)(void);
typedef pid_t (*wait_ty)(int *wstatus);

/* How many ways can each function fail? This *must* be kept up-to-date with
the FalliableFunc enum and the actual error list or issues will arise!! */

int NumFailureModes[12] = {6, 3, 6, 3, 2, 8, 4, 5, 2, 10, 3, 3};

/* These arrays store ways that we can reasonably fail for the given syscall.
   Note that these are not exhaustive--they are simply reasonable errnos to set
   for the given syscall */

int OpenFailures[] = {EACCES, EFAULT, ELOOP, ENOMEM, EPERM, EINTR};
int CloseFailures[] = {EBADF, EINTR, EIO};
int FopenFailures[] = {EACCES, EFAULT, ELOOP, ENOMEM, EPERM, EINTR};
int FcloseFailures[] = {EBADF, EINTR, EIO};
int FseekFailures[] = {EBADF, EINVAL};
int CreatFailures[] = {EACCES, EFAULT, ELOOP,  ENOMEM,
                       EPERM,  EINTR,  EINVAL, ENAMETOOLONG};
int Dup2Failures[] = {EBADF, EBUSY, EINTR, EMFILE};
int GetcwdFailures[] = {EACCES, EFAULT, EINVAL, ENAMETOOLONG, ENOMEM};
int GetlineFailures[] = {EINVAL, ENOMEM};
int ExecvFailures[] = {E2BIG,  EACCES, EFAULT, EIO,   ENAMETOOLONG,
                       ENFILE, ENOENT, ENOMEM, EPERM, ETXTBSY};
int ForkFailures[] = {EAGAIN, ENOMEM, ENOSYS};
int WaitFailures[] = {ECHILD, EINTR, EINVAL};

int *FailureModes[12] = {OpenFailures,   CloseFailures,  FopenFailures,
                         FcloseFailures, FseekFailures,  CreatFailures,
                         Dup2Failures,   GetcwdFailures, GetlineFailures,
                         ExecvFailures,  ForkFailures,   WaitFailures};

/* Returns an appropriate failure for the given function call */
int randomize_failure_kind(enum FalliableFunc funcCalled) {
  int idx = (int)funcCalled;
  int *validFailureModes = FailureModes[idx];
  int failIdx = rand() % NumFailureModes[idx];
  return validFailureModes[failIdx];
}

int syscall_fail(enum FalliableFunc funcCalled) {
  int myErrno = randomize_failure_kind(funcCalled);
  errno = myErrno;
  switch (funcCalled) {
  case Open:
  case Close:
  case Creat:
  case Fseek:
  case Dup2:
  case Getline:
  case Execv:
  case Fork:
  case Wait:
    return -1;
  case Fopen:
  case Getcwd:
    return 0;
  case Fclose:
    return EOF;
  default:
    assert(0 &&
           "syscall_fail called with unknown function type. Internal bug?");
  }
}

/* Functions for writing and controlling the PID files (which signal whether the
process successfully completed or not), as well as the directory that the
files are written in. Note: none of these functions are thread-safe because
I've given up pretending to care. If utcsh ever goes multithreaded or 
signal-based, these will need to be rewritten. */

// We may execute a chdir (e.g. from cd command) so store the first location used
// as the ebb dir path. Also provide static buffers so that filename generation
// does not require us to call malloc()
char ebb_dir_path[1024];   // The path to the directory containing ebb files.
char ebb_fpath[2048];      // A scratch buffer for computing ebb file paths.

// Obtains the path to the ebb directory via singleton pattern: if it already
// exists, just return it. Otherwise, computes (and possibly creates) the dir.
char* get_ebb_output_directory() {
  withinEBB = true;
  char cwd[1000];
  memset(cwd, 0, 1000);
  if (strlen(ebb_dir_path) == 0){
    // Congratulations, you played yourself. Use dlsym to avoid triggering our
    // own getcwd error.
    errno = 0;
    getcwd_ty getcwdFunc = dlsym(RTLD_NEXT, "getcwd");
    getcwdFunc(cwd, sizeof cwd);
    if (errno != 0){
      fprintf(stderr, "Could not get the current working directory, error %d\n", errno);
      abort();
    }
    sprintf(ebb_dir_path, "%s/.ebb_files", cwd);
  }

  mkdir(ebb_dir_path, 0755);
  struct stat st;
  stat(ebb_dir_path, &st);
  if (!S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Could not open directory %s, error %d\n", ebb_dir_path, errno);
    abort();
  }
  
  withinEBB = false;
  return ebb_dir_path; 
}

char* gen_ebb_filepath(char* filename){
  withinEBB = true;
  if (strlen(filename) > 1023){
      // I can promise this isn't going to work.
      kill(getpid(), SIGABRT);
  }
  sprintf(ebb_fpath, "%s/%s", get_ebb_output_directory(), filename);
  withinEBB = false;
  return ebb_fpath;
}

char* ebb_pid_filename() {
  pid_t mypid = getpid();
  char pid_s[100];
  sprintf(pid_s, "%d", mypid);
  return gen_ebb_filepath(pid_s);
}

void delete_ebb_pid_file(){
  char* pid_file = ebb_pid_filename();
  remove(pid_file);
}

// Creates the PID file for evilboombox and registers a function to delete it 
// on normal process termination via atexit(). This should be safe to call
// recursively and multiple times (ignores errors from file creation)
void create_ebb_pid_file(){
  char* pid_file = ebb_pid_filename();

  /* We can recursively enter this function, e.g. when a call to a creat causes 
  a malloc call. This wouldn't be a problem, but atexit() is protected by a 
  non-recursive mutex, so recursively entering this function causes a deadlock.

  Solution is somewhat hacky: if the pid file already exists, we just exit,
  avoiding the deadlock, under the assumption that whoever created the file
  either already registered the hook, or is going to shortly (i.e. when we 
  return to them). */
  if(access(pid_file, F_OK) == 0){
    return;
  }

  creat_ty creatFunc = dlsym(RTLD_NEXT, "creat");
  creatFunc(pid_file, S_IRWXU);

  // Register a hook to delete this file once we exit.
  if (atexit(delete_ebb_pid_file)){
    fprintf(stderr, "Could not register atexit hook for pid file\n");
    abort();
  }
}


/** The structures for controlling when these syscalls explode */
static _Atomic bool countdownIsInit; // Has the value of `counter` been set?
static _Atomic bool exploded;        // Has the failed call occurred?
static _Atomic int syscallCtr; // Number of calls before failure.

#define SYSCALL_COUNTDOWN_TIMER_NAME "EBB_SYSCALL_CTR"
#define SYSCALL_TRIGGERED_FILENAME ".ebb_syscall_fired"

static bool check_and_dec_ctr() {
  // If it has not been done yet, initialize our countdown from the environ
  if (!countdownIsInit) {
    countdownIsInit = true;
    create_ebb_pid_file();

    char *ctdown_s = getenv(SYSCALL_COUNTDOWN_TIMER_NAME);
    int ctdown;
    if (!ctdown_s) {
      ctdown = -1;
    } else {
      ctdown = atoi(ctdown_s);
    }

    DEBUG_PRINT("Malloc initialized with countdown = %d\n", ctdown);

    if (ctdown < 0) {
      // Behave as if we have already triggered the faulty call
      syscallCtr = 0;
      exploded = true;
    } else {
      syscallCtr = ctdown;
      exploded = false;
    }
  }

  /* See if we should asplode the function on this call. If not, move us closer
     to the countdown. */
  if (!exploded) {
    if (syscallCtr == 0) {
      exploded = true;
      DEBUG_PRINT("BOOM. alloc has failed.\n");

      // Create our own creat to avoid having this syscall explode.
      creat_ty creatFunc = dlsym(RTLD_NEXT, "creat");
      creatFunc(gen_ebb_filepath(SYSCALL_TRIGGERED_FILENAME), S_IRWXU);
      return true;
    } else {
      --syscallCtr;
    }
  }
  return false;
}

int open(const char *pathname, int flags, ...) {
  withinEBB = true;
  int rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Open);
  } else {
    open_ty openFunc = dlsym(RTLD_NEXT, "open");
    rv = openFunc(pathname, flags);
  }
  withinEBB = false;
  return rv;
}

int close(int fd) {
  withinEBB = true;
  int rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Close);
  } else {
    close_ty closeFunc = dlsym(RTLD_NEXT, "close");
    rv = closeFunc(fd);
  }
  return rv;
}

FILE *fopen(const char *restrict pathname, const char *restrict mode) {
  withinEBB = true;
  FILE *rv = NULL;
  if (check_and_dec_ctr()) {
    syscall_fail(Fopen); // Just for setting errno
    rv = NULL;
  } else {
    fopen_ty fopenFunc = dlsym(RTLD_NEXT, "fopen");
    rv = fopenFunc(pathname, mode);
  }
  withinEBB = false;
  return rv;
}

int fclose(FILE *stream) {
  withinEBB = true;
  int rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Fclose);
  } else {
    fclose_ty fcloseFunc = dlsym(RTLD_NEXT, "fclose");
    rv = fcloseFunc(stream);
  }
  withinEBB = false;
  return rv;
}

int fseek(FILE *stream, long offset, int whence) {
  withinEBB = true;
  int rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Fseek);
  } else {
    fseek_ty fseekFunc = dlsym(RTLD_NEXT, "fseek");
    rv = fseekFunc(stream, offset, whence);
  }
  withinEBB = false;
  return rv;
}

int creat(const char *path, mode_t mode) {
  withinEBB = true;
  int rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Creat);
  } else {
    creat_ty creatFunc = dlsym(RTLD_NEXT, "creat");
    rv = creatFunc(path, mode);
  }
  withinEBB = false;
  return rv;
}
int dup2(int fd1, int fd2) {
  withinEBB = true;
  int rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Dup2);
  } else {
    dup2_ty dup2Func = dlsym(RTLD_NEXT, "dup2");
    rv = dup2Func(fd1, fd2);
  }
  withinEBB = false;
  return rv;
}
char *getcwd(char *buf, size_t size) {
  withinEBB = true;
  char *rv;
  if (check_and_dec_ctr()) {
    syscall_fail(Getcwd); // Just for setting ernno
    rv = NULL;
  } else {
    getcwd_ty getcwdFunc = dlsym(RTLD_NEXT, "getcwd");
    rv = getcwdFunc(buf, size);
  }
  withinEBB = false;
  return rv;
}
char *getwd(char *name) {
  (void)name;
  assert(
      0 &&
      "The manpage of getwd() says that this function is deprecated for security\
  reasons. Why are you still using it??");
}
ssize_t getline(char **restrict lineptr, size_t *restrict n,
                FILE *restrict stream) {
  withinEBB = true;
  ssize_t rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Getline);
  } else {
    getline_ty getlineFunc = dlsym(RTLD_NEXT, "getline");
    rv = getlineFunc(lineptr, n, stream);
  }
  withinEBB = false;
  return rv;
}
int execv(const char *pathname, char *const argv[]) {
  withinEBB = true;
  int rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Execv);
  } else {
    // We need to unset the LD_PRELOAD flag, or evilboombox will also infect the
    // child, which is not good if the new process is anything but utcsh.
    unsetenv("LD_PRELOAD");

    // Exec will unset our atexit() hook, and we've already made it to here
    // without an error, so we can just delete the pid file now.
    delete_ebb_pid_file();

    execv_ty execvFunc = dlsym(RTLD_NEXT, "execv");
    rv = execvFunc(pathname, argv);
  }
  withinEBB = false;
  return rv;
}
pid_t fork(void) {
  withinEBB = true;
  pid_t rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Fork);
  } else {
    fork_ty forkFunc = dlsym(RTLD_NEXT, "fork");
    rv = forkFunc();
    if (rv == 0){
      create_ebb_pid_file();
    }
  }
  withinEBB = false;
  return rv;
}
pid_t wait(int *wstatus) {
  withinEBB = true;
  pid_t rv;
  if (check_and_dec_ctr()) {
    rv = syscall_fail(Wait);
  } else {
    wait_ty waitFunc = dlsym(RTLD_NEXT, "wait");
    rv = waitFunc(wstatus);
  }
  withinEBB = false;
  return rv;
}
