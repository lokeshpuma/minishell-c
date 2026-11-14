#ifndef MINISHELL_SIGNALS_H
#define MINISHELL_SIGNALS_H

#include <signal.h>

extern volatile sig_atomic_t g_sigint_received;

void setup_shell_signals(void);
void setup_child_signals(void);
void ignore_signals(void);

#endif /* MINISHELL_SIGNALS_H */

