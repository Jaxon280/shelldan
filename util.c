#include "util.h"

void set_ignore()
{
    struct sigaction sact;

    sigemptyset(&sact.sa_mask);
    sact.sa_flags = SA_RESTART;

    sact.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &sact, NULL);

    sact.sa_handler = SIG_IGN;
    sigaction(SIGTTIN, &sact, NULL);

    sact.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &sact, NULL);
}

void set_default()
{
    struct sigaction sact;

    sigemptyset(&sact.sa_mask);
    sact.sa_flags = SA_RESTART;

    sact.sa_handler = SIG_DFL;
    sigaction(SIGTTOU, &sact, NULL);

    sact.sa_handler = SIG_DFL;
    sigaction(SIGTTIN, &sact, NULL);

    sact.sa_handler = SIG_DFL;
    sigaction(SIGTSTP, &sact, NULL);
}

void free_string(char *string)
{
    if (string != NULL)
        free(string);

    string = NULL;
}

bool is_builtin(char *cmd)
{
    for (size_t i = 0; i < n_builtins; i++)
    {
        if (strcmp(cmd, builtins[i]) == 0)
        {
            return true;
        }
    }
    return false;
}
