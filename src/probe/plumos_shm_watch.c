#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_SEGMENTS 16
#define DEFAULT_TIMEOUT_MS 10000
#define DEFAULT_INTERVAL_MS 200
#define DEFAULT_MAX_BYTES 128

struct shm_segment {
  int key;
  int shmid;
  int size;
  unsigned char *addr;
  unsigned char *last;
  int attached;
};

static int parse_int_arg(const char *name, const char *value, int min_value, int max_value,
                         int *out) {
  char *end = NULL;
  long n;

  errno = 0;
  n = strtol(value, &end, 10);
  if (errno || !end || *end || n < min_value || n > max_value) {
    fprintf(stderr, "error: invalid %s: %s\n", name, value);
    return 0;
  }
  *out = (int)n;
  return 1;
}

static long long now_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return 0;
  }
  return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
}

static int read_segments(struct shm_segment *segments, int max_segments) {
  FILE *f;
  char line[512];
  int count = 0;

  f = fopen("/proc/sysvipc/shm", "rb");
  if (!f) {
    fprintf(stderr, "error: cannot read /proc/sysvipc/shm: %s\n", strerror(errno));
    return 0;
  }
  if (!fgets(line, sizeof(line), f)) {
    fclose(f);
    return 0;
  }
  while (count < max_segments && fgets(line, sizeof(line), f)) {
    int key;
    int shmid;
    int perms;
    int size;
    if (sscanf(line, "%d %d %o %d", &key, &shmid, &perms, &size) != 4) {
      continue;
    }
    segments[count].key = key;
    segments[count].shmid = shmid;
    segments[count].size = size;
    segments[count].addr = NULL;
    segments[count].last = NULL;
    segments[count].attached = 0;
    count++;
  }
  fclose(f);
  return count;
}

static void dump_prefix(const unsigned char *buf, int size, int max_bytes) {
  int n = size < max_bytes ? size : max_bytes;
  for (int i = 0; i < n; i++) {
    printf("%02x", buf[i]);
    if (i + 1 < n) {
      putchar(' ');
    }
  }
}

static int attach_segment(struct shm_segment *segment, int max_bytes) {
  void *addr;

  addr = shmat(segment->shmid, NULL, SHM_RDONLY);
  if (addr == (void *)-1) {
    printf("segment key=%d shmid=%d size=%d attach=no errno=%d %s\n",
           segment->key, segment->shmid, segment->size, errno, strerror(errno));
    return 0;
  }
  segment->addr = (unsigned char *)addr;
  segment->last = (unsigned char *)calloc((size_t)segment->size, 1);
  if (!segment->last) {
    shmdt(addr);
    segment->addr = NULL;
    return 0;
  }
  memcpy(segment->last, segment->addr, (size_t)segment->size);
  segment->attached = 1;
  printf("segment key=%d shmid=%d size=%d attach=yes initial=", segment->key,
         segment->shmid, segment->size);
  dump_prefix(segment->last, segment->size, max_bytes);
  printf("\n");
  return 1;
}

static void detach_segments(struct shm_segment *segments, int count) {
  for (int i = 0; i < count; i++) {
    if (segments[i].attached && segments[i].addr) {
      shmdt(segments[i].addr);
    }
    free(segments[i].last);
    segments[i].addr = NULL;
    segments[i].last = NULL;
    segments[i].attached = 0;
  }
}

static void print_changes(struct shm_segment *segment, int sample, int max_bytes) {
  int changed = 0;
  int limit = segment->size < max_bytes ? segment->size : max_bytes;

  for (int i = 0; i < segment->size; i++) {
    if (segment->addr[i] != segment->last[i]) {
      changed = 1;
      break;
    }
  }
  if (!changed) {
    return;
  }
  printf("change sample=%d key=%d shmid=%d bytes=", sample, segment->key, segment->shmid);
  for (int i = 0; i < limit; i++) {
    if (segment->addr[i] == segment->last[i]) {
      continue;
    }
    printf("%d:%02x->%02x ", i, segment->last[i], segment->addr[i]);
  }
  printf("snapshot=");
  dump_prefix(segment->addr, segment->size, max_bytes);
  printf("\n");
  memcpy(segment->last, segment->addr, (size_t)segment->size);
}

static void usage(const char *argv0) {
  printf("Usage: %s [--timeout-ms MS] [--interval-ms MS] [--max-bytes N]\n", argv0);
  printf("Read-only SysV shared memory watcher for stock keymon/MainUI investigation.\n");
}

int main(int argc, char **argv) {
  struct shm_segment segments[MAX_SEGMENTS];
  int count;
  int timeout_ms = DEFAULT_TIMEOUT_MS;
  int interval_ms = DEFAULT_INTERVAL_MS;
  int max_bytes = DEFAULT_MAX_BYTES;
  long long deadline;
  int sample = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 300000, &timeout_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--interval-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--interval-ms", argv[++i], 10, 10000, &interval_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--max-bytes") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--max-bytes", argv[++i], 1, 4096, &max_bytes)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  memset(segments, 0, sizeof(segments));
  count = read_segments(segments, MAX_SEGMENTS);
  printf("plumOS shm watch\n");
  printf("segments=%d timeout_ms=%d interval_ms=%d max_bytes=%d\n", count, timeout_ms,
         interval_ms, max_bytes);
  for (int i = 0; i < count; i++) {
    attach_segment(&segments[i], max_bytes);
  }

  deadline = now_ms() + timeout_ms;
  while (now_ms() < deadline) {
    usleep((useconds_t)interval_ms * 1000U);
    sample++;
    for (int i = 0; i < count; i++) {
      if (segments[i].attached) {
        print_changes(&segments[i], sample, max_bytes);
      }
    }
    fflush(stdout);
  }
  detach_segments(segments, count);
  return 0;
}
