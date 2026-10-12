#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int (*real_sigaction)(int, const struct sigaction *, struct sigaction *);
static int segv_enabled;

static int watched(int sig) {
  return sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE;
}

static void dump_maps(void) {
  int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return;
  static char buf[262144];
  size_t len = 0;
  ssize_t n;
  while (len < sizeof buf - 1 && (n = read(fd, buf + len, sizeof buf - 1 - len)) > 0) len += (size_t)n;
  close(fd);
  buf[len] = 0;
  char *line = buf;
  while (line && *line) {
    char *next = strchr(line, '\n');
    if (next) *next = 0;
    if (strstr(line, "LLVM") || strstr(line, "lvp") || strstr(line, "gallium") || strstr(line, "zink") || strstr(line, "vulkan") || strstr(line, "libGL") || strstr(line, "dri")) {
      size_t l = strlen(line);
      if (write(2, line, l) < 0) return;
      if (write(2, "\n", 1) < 0) return;
    }
    line = next ? next + 1 : NULL;
  }
}

#if defined(__aarch64__)
#include <ucontext.h>
static void dump_regs(void *uc) {
  ucontext_t *u = uc;
  char line[160];
  for (int i = 0; i < 31; i += 4) {
    int n = snprintf(line, sizeof line, "== regs x%d=%llx x%d=%llx x%d=%llx x%d=%llx\n", i, (unsigned long long)u->uc_mcontext.regs[i], i + 1, (unsigned long long)u->uc_mcontext.regs[i + 1], i + 2 < 31 ? i + 2 : 30, (unsigned long long)u->uc_mcontext.regs[i + 2 < 31 ? i + 2 : 30], i + 3 < 31 ? i + 3 : 30, (unsigned long long)u->uc_mcontext.regs[i + 3 < 31 ? i + 3 : 30]);
    if (n > 0 && write(2, line, (size_t)n) < 0) return;
  }
  int n = snprintf(line, sizeof line, "== regs pc=%llx sp=%llx\n", (unsigned long long)u->uc_mcontext.pc, (unsigned long long)u->uc_mcontext.sp);
  if (n > 0 && write(2, line, (size_t)n) < 0) return;
}
#else
static void dump_regs(void *uc) { (void)uc; }
#endif

static void segv_handler(int sig, siginfo_t *si, void *uc) {
  char head[200];
  int n = snprintf(head, sizeof head, "\n== bl-segv: signal %d code %d addr %p pid %d\n", sig, si->si_code, si->si_addr, getpid());
  if (n > 0 && write(2, head, (size_t)n) < 0) _exit(139);
  dump_regs(uc);
  void *frames[64];
  int count = backtrace(frames, 64);
  backtrace_symbols_fd(frames, count, 2);
  if (write(2, "== bl-segv: relevant mappings\n", 30) < 0) _exit(139);
  dump_maps();
  if (write(2, "== bl-segv: end\n", 16) < 0) _exit(139);
  struct sigaction dfl;
  memset(&dfl, 0, sizeof dfl);
  dfl.sa_handler = SIG_DFL;
  real_sigaction(sig, &dfl, NULL);
  raise(sig);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  if (!real_sigaction) real_sigaction = dlsym(RTLD_NEXT, "sigaction");
  if (segv_enabled && act && watched(signum)) {
    if (oldact) real_sigaction(signum, NULL, oldact);
    return 0;
  }
  return real_sigaction(signum, act, oldact);
}

__attribute__((constructor)) static void segv_init(void) {
  if (!getenv("BL_SEGV")) return;
  real_sigaction = dlsym(RTLD_NEXT, "sigaction");
  if (!real_sigaction) return;
  void *warm[4];
  backtrace(warm, 4);
  static char altstack[65536];
  stack_t ss;
  ss.ss_sp = altstack;
  ss.ss_size = sizeof altstack;
  ss.ss_flags = 0;
  sigaltstack(&ss, NULL);
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = segv_handler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  int sigs[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE};
  for (unsigned i = 0; i < sizeof sigs / sizeof sigs[0]; i++) real_sigaction(sigs[i], &sa, NULL);
  segv_enabled = 1;
}
