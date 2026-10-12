#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef RTLD_DEEPBIND
#define RTLD_DEEPBIND 0x00008
#endif

static void *(*real_dlopen)(const char *, int);

void *dlopen(const char *file, int flags) {
  if (!real_dlopen) real_dlopen = dlsym(RTLD_NEXT, "dlopen");
  if (file && getenv("BL_VK_DEEPBIND") && strstr(file, "/linux_vulkan_drivers/")) {
    flags |= RTLD_DEEPBIND;
    char line[400];
    int n = snprintf(line, sizeof line, "== bl-deepbind: dlopen %s flags=%x\n", file, (unsigned)flags);
    if (n > 0 && write(2, line, (size_t)n) < 0) return real_dlopen(file, flags);
  }
  return real_dlopen(file, flags);
}
