#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static void usage(const char *argv0) {
  printf("Usage: %s [--host HOST] [--port PORT] [--newline] [--repeat N] [--delay-ms MS] [--recv-ms MS] MESSAGE\n",
         argv0);
  printf("Default host/port target RetroArch network commands: 127.0.0.1:55355\n");
}

static long parse_long(const char *value) {
  char *end = NULL;
  long result = strtol(value, &end, 10);
  if (!value[0] || (end && *end)) {
    return -1;
  }
  return result;
}

int main(int argc, char **argv) {
  const char *host = "127.0.0.1";
  int port = 55355;
  int add_newline = 0;
  long repeat = 1;
  long delay_ms = 250;
  long recv_ms = 0;
  const char *message = NULL;
  char send_buf[2048];
  const char *payload = NULL;
  size_t payload_len = 0;
  struct sockaddr_in addr;
  int fd;
  ssize_t sent;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--host") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: --host requires a value\n");
        return 2;
      }
      host = argv[++i];
    } else if (strcmp(argv[i], "--port") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: --port requires a value\n");
        return 2;
      }
      long parsed = parse_long(argv[++i]);
      if (parsed <= 0 || parsed > 65535) {
        fprintf(stderr, "error: invalid port\n");
        return 2;
      }
      port = (int)parsed;
    } else if (strcmp(argv[i], "--newline") == 0) {
      add_newline = 1;
    } else if (strcmp(argv[i], "--repeat") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: --repeat requires a value\n");
        return 2;
      }
      repeat = parse_long(argv[++i]);
      if (repeat <= 0 || repeat > 1000) {
        fprintf(stderr, "error: invalid repeat count\n");
        return 2;
      }
    } else if (strcmp(argv[i], "--delay-ms") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: --delay-ms requires a value\n");
        return 2;
      }
      delay_ms = parse_long(argv[++i]);
      if (delay_ms < 0 || delay_ms > 60000) {
        fprintf(stderr, "error: invalid delay\n");
        return 2;
      }
    } else if (strcmp(argv[i], "--recv-ms") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: --recv-ms requires a value\n");
        return 2;
      }
      recv_ms = parse_long(argv[++i]);
      if (recv_ms < 0 || recv_ms > 60000) {
        fprintf(stderr, "error: invalid receive timeout\n");
        return 2;
      }
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else if (!message) {
      message = argv[i];
    } else {
      fprintf(stderr, "error: unexpected argument: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }

  if (!message) {
    fprintf(stderr, "error: missing MESSAGE\n");
    usage(argv[0]);
    return 2;
  }

  if (add_newline) {
    int written = snprintf(send_buf, sizeof(send_buf), "%s\n", message);
    if (written < 0 || (size_t)written >= sizeof(send_buf)) {
      fprintf(stderr, "error: message too long\n");
      return 2;
    }
    payload = send_buf;
    payload_len = (size_t)written;
  } else {
    payload = message;
    payload_len = strlen(message);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  if (inet_aton(host, &addr.sin_addr) == 0) {
    fprintf(stderr, "error: host must be a numeric IPv4 address: %s\n", host);
    return 1;
  }

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    fprintf(stderr, "error: socket: %s\n", strerror(errno));
    return 1;
  }
  for (long i = 0; i < repeat; i++) {
    sent = sendto(fd, payload, payload_len, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent != (ssize_t)payload_len) {
      fprintf(stderr, "error: sendto: %s\n", sent < 0 ? strerror(errno) : "short send");
      close(fd);
      return 1;
    }
    if (i + 1 < repeat && delay_ms > 0) {
      usleep((useconds_t)delay_ms * 1000);
    }
  }

  if (recv_ms > 0) {
    fd_set readfds;
    struct timeval timeout;
    char reply[2048];
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    timeout.tv_sec = recv_ms / 1000;
    timeout.tv_usec = (recv_ms % 1000) * 1000;
    int ready = select(fd + 1, &readfds, NULL, NULL, &timeout);
    if (ready < 0) {
      fprintf(stderr, "error: select: %s\n", strerror(errno));
      close(fd);
      return 1;
    }
    if (ready == 0) {
      fprintf(stderr, "error: receive timeout\n");
      close(fd);
      return 1;
    }
    ssize_t got = recv(fd, reply, sizeof(reply) - 1, 0);
    if (got < 0) {
      fprintf(stderr, "error: recv: %s\n", strerror(errno));
      close(fd);
      return 1;
    }
    reply[got] = '\0';
    printf("reply bytes=%zd %s", got, reply);
    if (got == 0 || reply[got - 1] != '\n') {
      printf("\n");
    }
  }
  close(fd);

  printf("sent host=%s port=%d bytes=%zu repeat=%ld message=%s\n", host, port, payload_len,
         repeat, message);
  return 0;
}
