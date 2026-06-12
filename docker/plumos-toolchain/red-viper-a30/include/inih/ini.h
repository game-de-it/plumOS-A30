#ifndef PLUMOS_RED_VIPER_A30_INIH_STUB_H
#define PLUMOS_RED_VIPER_A30_INIH_STUB_H

typedef int (*ini_handler)(void *user, const char *section, const char *name,
                           const char *value);

static inline int ini_parse(const char *filename, ini_handler handler, void *user) {
  (void)filename;
  (void)handler;
  (void)user;
  return -1;
}

#endif
