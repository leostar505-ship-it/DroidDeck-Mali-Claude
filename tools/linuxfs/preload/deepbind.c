#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#ifndef RTLD_DEEPBIND
#define RTLD_DEEPBIND 0x00008
#endif

static void *(*real_dlopen)(const char *, int);

void *dlopen(const char *file, int flags) {
  if (!real_dlopen) real_dlopen = dlsym(RTLD_NEXT, "dlopen");
  if (file && getenv("BL_VK_DEEPBIND") && strstr(file, "/linux_vulkan_drivers/")) flags |= RTLD_DEEPBIND;
  return real_dlopen(file, flags);
}
