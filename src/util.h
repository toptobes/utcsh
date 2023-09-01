#ifndef UTCSH_UTILS_H
#define UTCSH_UTILS_H

#include <stdbool.h>
#include "utcsh.r"

#define MAX_CHARS_PER_CMDLINE 2048
#define MAX_WORDS_PER_CMDLINE 256
#define MAX_CHARS_PER_CMD 512
#define MAX_WORDS_PER_CMD 64
#define MAX_ENTRIES_IN_SHELLPATH 256

/**
 * Modify the shell_paths global. Will run until all MAX_ENTRIES_IN_SHELLPATH
 * elements have been set or a NULL char* is found in newPaths. Returns 1 on
 * success and zero on error *
 */
int set_shell_path(char **newPaths);

/** Returns 1 if this is an absolute path, 0 otherwise */
int is_absolute_path(char *path);

/** Determines whether an executable file with the name `filename` exists in the
 * directory named `dirname`.
 *
 * If so, returns a char* with the full path to the file. This pointer MUST
 * be freed by the calling function.
 *
 * If no such file exists in the directory, or if the file exists but is not
 * executable, this function returns NULL.
 *
 * The `verbose` flag should usually be set to false. If set to true, it will
 * cause the function to print a log of what it is doing to standard out. This
 * will cause the shell to fail all automated tests that cause this function to
 * be called, but may be useful for debugging.
 * */
char *exe_exists_in_dir(const char *dirname, const char *filename,
                        bool verbose);

#define autofree __attribute__((cleanup(free_pointer)))
void free_pointer(void *ptr);

#define autoclose __attribute__((cleanup(close_fd)))
void close_fd(int *fd);

#define let __auto_type

int num_whitespaces(const char *str);

#define scold_user(...) ({ int __fprintf_ret = fprintf(stderr, __VA_ARGS__); if (__fprintf_ret < 0) exit(2); });
#define ppanic(e) ({ perror(e); exit(1); });

enum { CHILD_PROCESS };

void printcmds(Command *cmds, int ncmds);

#endif//UTILS
