#include "signals.h"
#include <unistd.h>
#include <string.h>

volatile sig_atomic_t g_sigint_received = 0;

static void sigint_handler(int sig) {
    (void)sig;
    g_sigint_received = 1;
    /* Write newline directly to terminal in an async-signal-safe way */
    const char nl[] = "\n";
    (void)write(STDERR_FILENO, nl, sizeof(nl) - 1);
}

void setup_shell_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* Do not use SA_RESTART so blocking reads (getline) fail with EINTR */
    sigaction(SIGINT, &sa, NULL);

    /* Shell should ignore SIGQUIT and SIGTSTP */
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}

void setup_child_signals(void) {
    /* Reset signals to default in child processes */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

void ignore_signals(void) {
    signal(SIGINT, SIG_IGN);
}

