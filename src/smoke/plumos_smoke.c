#include <stdio.h>
#include <sys/utsname.h>
#include <unistd.h>

int main(int argc, char **argv) {
  struct utsname uts;

  (void)argc;

  printf("plumOS A30 smoke binary\n");
  printf("argv0: %s\n", argv[0] ? argv[0] : "(null)");
  printf("pid: %ld\n", (long)getpid());
  printf("pointer_size: %zu\n", sizeof(void *));

  if (uname(&uts) == 0) {
    printf("sysname: %s\n", uts.sysname);
    printf("release: %s\n", uts.release);
    printf("machine: %s\n", uts.machine);
  } else {
    perror("uname");
    return 1;
  }

  return 0;
}
