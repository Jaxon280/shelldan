#include "job.h"
#include "parser.h"
#include "process.h"
#include "util.h"

Shell *shell;

int main()
{
    // setvbuf(stdout, NULL, _IONBF, 0); //test

    shell = (Shell *)calloc(1, sizeof(Shell));

    set_ignore();

    while (1)
    {
        size_t line_size = 0;
        Token *tokens = (Token *)calloc(1, sizeof(Token));

        printf("shellman$ ");
        line_size = tokenize_line(tokens);

        wait_back_job();

        shell->cur_job = new_job(line_size);
        if (parse(shell->cur_job, tokens) == -1)
        {
            printf("-shellman: failed to parse tokens\n");
            goto POSTPROCESSING;
        }

        if (shell->cur_job->job_mode != BUILTIN_MODE && shell->cur_job != NULL)
        {
            insert_job(shell->cur_job);
        }
        run_job(shell->cur_job);

        switch (shell->cur_job->job_mode)
        {
        case BACK_MODE:
            goto BACKGROUND;
            break;

        case FORE_MODE:
            goto FOREGROUND;
            break;

        default:
            goto POSTPROCESSING;
        }

    FOREGROUND:
        // To prevent SIGTTIN, tcsetpgrp() for setting current job's pgrp to foreground process is called in run_job()
        shell->cur_job->job_state = Running;

        wait_fore_job(shell->cur_job);

        if (tcsetpgrp(STDIN_FILENO, getpgid((pid_t)0)) == -1)
        {
            perror("tcsetpgrp");
        }
        goto POSTPROCESSING;

    BACKGROUND:
        shell->cur_job->job_state = Running;
        printf("[%d] %d %s\n", shell->cur_job->id, shell->cur_job->pgid, shell->cur_job->line);
        goto POSTPROCESSING;

    POSTPROCESSING:
        free_token(tokens);
        free_jobs(); // free jobs and finished_job_list
        printf("\n");
    }

    exit(EXIT_SUCCESS);
}
