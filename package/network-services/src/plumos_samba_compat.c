#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif

typedef int (*setsockopt_fn)(int, int, int, const void *, socklen_t);

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
  static setsockopt_fn real_setsockopt = NULL;

  if (real_setsockopt == NULL) {
    real_setsockopt = (setsockopt_fn)dlsym(RTLD_NEXT, "setsockopt");
    if (real_setsockopt == NULL) {
      errno = ENOSYS;
      return -1;
    }
  }

  if (real_setsockopt(sockfd, level, optname, optval, optlen) == 0) {
    return 0;
  }

  if (level == SOL_SOCKET && optname == SO_REUSEPORT && errno == ENOPROTOOPT) {
    errno = 0;
    return 0;
  }

  return -1;
}
