#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "signal.h"
#include <unistd.h>

#define MAX_BUFFER_SIZE 128
#define MAX_ARG_SIZE 8

typedef struct process Process;
struct process
{
    pid_t pid;
    Process *next;
    char *cmd;
    char *args[MAX_ARG_SIZE];
    char *read_filepath;
    char *write_filepath;
    int read_fd;
    int write_fd;
    int status;
};

// change of JobState is: (Job is created) -> Pending -> (Running <-> Stopped) -> Done -> (Job is finished)
typedef enum JobState JobState;
enum JobState
{
    Pending,
    Running,
    Stopped,
    Done
};

typedef enum JobMode JobMode;
enum JobMode
{
    FORE_MODE,
    BACK_MODE,
    BUILTIN_MODE
};

typedef struct job Job;
struct job
{
    int id;
    pid_t pgid;
    char *line;
    JobState job_state;
    JobMode job_mode;
    Process *process_queue; // the first one in linked list
    int running_procs;      // The total number of running process. If this is reduced to 0, this job can said to be "finished".
    Job *next;
};

typedef struct shell Shell;
struct shell
{
    Job *jobs;
    Job *finished_jobs;
    Job *cur_job;
};

typedef enum
{
    NONE,
    PIPE,           // <CMD> ... ( "<" or ">" <FILE> ) "|" <CMD> ...
    LEFT_REDIRECT,  // <CMD> ... "<" <FILE(FREAD)>
    RIGHT_REDIRECT, // <CMD> ... ">" <FILE(FWRITE)>
    BACKGROUND,     // <CMD> ... "&"\n <--- "&" has to come to the end of input.
    CMD,            // <CMD> ( <ARG> <ARG> ... )
    ARG,
    BUILTIN_CMD, // <BUILTIN_CMD> (<ARG> <ARG> ...)
    FILE_PATH
} TokenLabel;

typedef struct token Token;
struct token
{
    TokenLabel label;
    Token *prev;
    Token *next;
    char *string;
    int arg_order;
};

Shell *shell = NULL;

static const char *builtins[] = {
    "jobs", "fg", "bg"};
static const size_t n_builtins = sizeof(builtins) / sizeof(char *);

Token *new_token(Token *cur_token)
{
    Token *new_token = (Token *)calloc(1, sizeof(Token));
    cur_token->next = new_token;
    new_token->prev = cur_token;
    return new_token;
}

Process *new_process(Process *cur_process)
{
    Process *new_process = (Process *)calloc(1, sizeof(Process));
    cur_process->next = new_process;
    return new_process;
}

void tokenize(Token *token, char *buffer)
{
    if (token->prev == NULL)
    {
        token->arg_order = 1;
        for (size_t i = 0; i < n_builtins; i++)
        {
            if (strcmp(buffer, builtins[i]) == 0)
            {
                token->label = BUILTIN_CMD;
                return;
            }
        }

        token->label = CMD;
        return;
    }

    if (strcmp(buffer, "|") == 0)
    {
        token->label = PIPE;
    }
    else if (strcmp(buffer, ">") == 0)
    {
        token->label = RIGHT_REDIRECT;
    }
    else if (strcmp(buffer, "<") == 0)
    {
        token->label = LEFT_REDIRECT;
    }
    else if (strcmp(buffer, "&") == 0)
    {
        token->label = BACKGROUND;
    }
    else if (token->prev->label == LEFT_REDIRECT || token->prev->label == RIGHT_REDIRECT)
    {
        token->label = FILE_PATH;
    }
    else if (token->prev->arg_order > 0)
    {
        token->arg_order = token->prev->arg_order + 1;
        token->label = ARG;
    }
    else
    {
        token->arg_order = 1;
        for (size_t i = 0; i < n_builtins; i++)
        {
            if (strcmp(buffer, builtins[i]) == 0)
            {
                token->label = BUILTIN_CMD;
                return;
            }
        }

        token->label = CMD;
    }
}

size_t getstdin(Token *token)
{
    int c = 0;
    int char_count = 0;
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, '\0', sizeof(buffer));
    size_t line_size = 0;
    size_t token_size;

    while (1)
    {
        c = getchar();

        if (c == ' ')
        {
            if (char_count > 0)
            {
                tokenize(token, buffer);

                token_size = (strlen(buffer) + 1) * sizeof(char);
                line_size += (token_size + 1);
                token->string = (char *)malloc(token_size);
                strcpy(token->string, buffer);

                token = new_token(token);

                memset(buffer, '\0', sizeof(buffer));
                char_count = 0;
            }
            else
            {
                continue;
            }
        }
        else if (c == '\n')
        {
            if (char_count > 0)
            {
                tokenize(token, buffer);

                token_size = (strlen(buffer) + 1) * sizeof(char);
                line_size += (token_size + 1);
                token->string = (char *)malloc(token_size);
                strcpy(token->string, buffer);

                token = new_token(token);

                memset(buffer, '\0', sizeof(buffer));
                char_count = 0;
            }

            token->label = NONE;
            break;
        }
        else
        {
            buffer[char_count] = c;
            char_count++;
        }
    }

    return line_size;
}

void parse(Job *job, Token *token)
{
    Token *cur_token = token;
    Process *cur_process = job->process_queue;

    while (cur_token->label != NONE)
    {
        switch (cur_token->label)
        {
        case CMD: // <CMD> ( <ARG> <ARG> ... )
            cur_process->cmd = (char *)malloc(sizeof(cur_token->string));
            strcpy(cur_process->cmd, cur_token->string);
            break;

        case BUILTIN_CMD:
            cur_process->cmd = (char *)malloc(sizeof(cur_token->string));
            strcpy(cur_process->cmd, cur_token->string);
            job->job_mode = BUILTIN_MODE;
            break;

        case ARG:
            if (cur_token->arg_order - 2 >= MAX_ARG_SIZE)
            {
                printf("Too many args");
                exit(1);
            }

            cur_process->args[cur_token->arg_order - 2] = (char *)malloc(sizeof(cur_token->string));
            strcpy(cur_process->args[cur_token->arg_order - 2], cur_token->string);
            break;

        case FILE_PATH:
            if (cur_token->prev->label == LEFT_REDIRECT)
            {
                cur_process->read_filepath = (char *)malloc(sizeof(cur_token->string));
                strcpy(cur_process->read_filepath, cur_token->string);
            }
            else if (cur_token->prev->label == RIGHT_REDIRECT)
            {
                cur_process->write_filepath = (char *)malloc(sizeof(cur_token->string));
                strcpy(cur_process->write_filepath, cur_token->string);
            }
            else
            {
                printf("Invalid operator");
                exit(1);
            }
            break;

        case PIPE:                                  // <CMD> ... ( "<" or ">" <FILE> ) "|" <CMD> ...
            cur_process = new_process(cur_process); // new_process() is invoked only here, because new CMD is guaranteed to come just after PIPE.
            break;

        case LEFT_REDIRECT: // <CMD> ... "<" <FILE(FREAD)>
            break;

        case RIGHT_REDIRECT: // <CMD> ... ">" <FILE(FWRITE)>
            break;

        case BACKGROUND: // <CMD> ... "&"\n <--- "&" has to come to the end of input.
            job->job_mode = BACK_MODE;
            break;

        default:
            break;
        }

        strcat(job->line, cur_token->string);
        if (cur_token->next->label != NONE)
        {
            strcat(job->line, " ");
        }

        cur_token = cur_token->next;
    }

    cur_process->next = NULL;
}

void free_token(Token *token)
{
    Token *cur_token, *next_token;
    for (cur_token = token; cur_token != NULL; cur_token = next_token)
    {
        if (cur_token->string != NULL)
        {
            free(cur_token->string);
        }

        next_token = cur_token->next;
        free(cur_token);
    }
}

// void handle_signal(int signum)
// {
//     if (signum == SIGINT)
//     {
//         char input[8];
//         printf("\ndo you really want to terminate shellman? [y/n]: ");

//         while (1)
//         {
//             scanf("%s", input);
//             if (strcmp(input, "y") == 0)
//             {
//                 printf("\nshellman has been terminated");
//                 kill(0, SIGKILL);
//             }
//             else if (strcmp(input, "n") == 0)
//             {
//                 return;
//             }
//             else
//             {
//                 printf("\nsorry, please type \"y\" or \"n\" again: ");
//                 continue;
//             }
//         }
//     }
// }

void ignore_signal()
{
    sigset_t sigmask;
    sigemptyset(&sigmask);
    struct sigaction sig;
    sig.sa_mask = sigmask;
    sig.sa_flags = SA_RESTART;

    sig.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &sig, NULL);

    sig.sa_handler = SIG_IGN;
    sigaction(SIGTTIN, &sig, NULL);

    sig.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &sig, NULL);

    // sig.sa_handler = &handle_signal;
    // sig.sa_handler = SIG_IGN;
    // sigaction(SIGINT, &sig, NULL);
}

void default_signal()
{
    sigset_t sigmask;
    sigemptyset(&sigmask);
    struct sigaction sig;
    sig.sa_mask = sigmask;
    sig.sa_flags = SA_RESTART;

    sig.sa_handler = SIG_DFL;
    sigaction(SIGTTOU, &sig, NULL);

    sig.sa_handler = SIG_DFL;
    sigaction(SIGTTIN, &sig, NULL);

    sig.sa_handler = SIG_DFL;
    sigaction(SIGTSTP, &sig, NULL);

    // sig.sa_handler = SIG_DFL;
    // sigaction(SIGINT, &sig, NULL);
}

void jobs(char **args)
{
    Job *cur_job;
    char state[8];

    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (cur_job->job_mode != BUILTIN_MODE)
        {
            switch (cur_job->job_state)
            {
            case Running:
                strcpy(state, "Running");
                break;

            case Stopped:
                strcpy(state, "Stopped");
                break;

            case Done:
                strcpy(state, "Done");
                break;

            default:
                break;
            }

            printf("[%d] %s %s\n", cur_job->id, state, cur_job->line);
        }
    }
}

void fg(char **args)
{
    Job *cur_job;
    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (cur_job->id == atoi(args[0]) && cur_job->job_state == Stopped)
        {
            shell->cur_job = cur_job;
            shell->cur_job->job_mode = FORE_MODE;
            kill(shell->cur_job->pgid, SIGCONT);
            printf("[%d] %s\n", shell->cur_job->id, shell->cur_job->line);
            return;
        }
    }

    printf("No job_id: %d.", atoi(args[0]));
}

void bg(char **args)
{
    Job *cur_job;
    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (cur_job->id == atoi(args[0]) && cur_job->job_state == Stopped)
        {
            shell->cur_job = cur_job;
            shell->cur_job->job_mode = BACK_MODE;
            kill(shell->cur_job->pgid, SIGCONT);
            printf("[%d] %s\n", shell->cur_job->id, shell->cur_job->line);
            return;
        }
    }

    printf("No job_id: %d.", atoi(args[0]));
}

void run_command(Process *command)
{
    if (strcmp(command->cmd, "jobs") == 0)
    {
        jobs(command->args);
    }
    else if (strcmp(command->cmd, "fg") == 0)
    {
        fg(command->args);
    }
    else if (strcmp(command->cmd, "bg") == 0)
    {
        bg(command->args);
    }
}

void run_job(Job *job)
{
    int status;
    pid_t pid, wait_pid;

    if (job->job_mode == BUILTIN_MODE)
    {
        run_command(job->process_queue);
        return;
    }

    Process *process;
    for (process = job->process_queue; process != NULL; process = process->next)
    {
        int read_fd = 0, write_fd = 0;
        if (process->read_filepath != NULL)
        {
            if (read_fd != 0)
                close(read_fd);

            if ((read_fd = open(process->read_filepath, O_RDONLY)) == -1)
            {
                perror("open file");
                exit(1);
            }
            process->read_fd = read_fd;
        }

        if (process->write_filepath != NULL)
        {
            if (write_fd != 0)
                close(write_fd);

            if ((write_fd = open(process->write_filepath, O_WRONLY | O_TRUNC)) == -1)
            {
                perror("open file");
                exit(1);
            }
            process->write_fd = write_fd;
        }

        if (process->next)
        {
            int pipe_fd[2];
            pipe(pipe_fd);
            process->write_fd = pipe_fd[1];
            process->next->read_fd = pipe_fd[0];
        }

        switch (pid = fork())
        {
        case -1:
            perror("fork child process");
            break;

        case 0:
            if (process->read_fd)
            {
                dup2(process->read_fd, STDIN_FILENO);
                close(process->read_fd);
            }

            if (process->write_fd)
            {
                dup2(process->write_fd, STDOUT_FILENO);
                close(process->write_fd);
            }

            default_signal();

            if (execv(process->cmd, process->args) == -1)
            {
                perror("exec child process");
                _exit(EXIT_FAILURE);
            }
            break;

        default:
            if (process->read_fd)
            {
                close(process->read_fd);
            }
            if (process->write_fd)
            {
                close(process->write_fd);
            }

            job->running_procs++;
            process->pid = pid;
            if (!job->pgid)
            {
                job->pgid = pid;
                setpgid(pid, pid);
            }
            else
            {
                setpgid(pid, job->pgid);
            }

            if (job->job_mode == FORE_MODE && job->pgid == pid)
            {
                if (tcsetpgrp(STDIN_FILENO, shell->cur_job->pgid) == -1)
                {
                    perror("tcsetpgrp");
                }
            }

            break;
        }
    }
}

int set_jobid()
{
    Job *cur_job;
    int max = 0;
    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (cur_job->job_mode != BUILTIN_MODE && max < cur_job->id)
        {
            max = cur_job->id;
        }
    }

    return max + 1;
}

Job *new_job(size_t byte_size)
{
    Job *new_job = (Job *)calloc(1, sizeof(Job));
    new_job->id = set_jobid();
    new_job->job_mode = FORE_MODE;
    new_job->job_state = Pending;
    new_job->line = (char *)calloc(1, byte_size);
    new_job->running_procs = 0;
    new_job->process_queue = (Process *)calloc(1, sizeof(Process));

    return new_job;
}

void append_job(Job *new_job)
{
    new_job->next = shell->jobs;
    shell->jobs = new_job;
}

void delete_job(int job_id)
{
    Job *cur_job = NULL, *prev_job = NULL;

    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (cur_job->id == job_id)
        {
            if (prev_job == NULL)
            {
                shell->jobs = cur_job->next;
                cur_job->next = NULL;
                break;
            }

            prev_job->next = cur_job->next;
            cur_job->next = NULL;
            break;
        }
        prev_job = cur_job;
    }
}

void append_finished_job(Job *finished_job)
{
    if (finished_job->next != NULL)
    {
        printf("This job can't be added to shell->finished_jobs because it is still in shell->jobs.");
        exit(1);
    }

    finished_job->next = shell->finished_jobs;
    shell->finished_jobs = finished_job;
}

void free_jobs()
{
    Job *cur_job, *next_job = NULL;
    Process *cur_proc, *next_proc = NULL;
    for (cur_job = shell->finished_jobs; cur_job != NULL; cur_job = next_job)
    {
        for (cur_proc = cur_job->process_queue; cur_proc != NULL; cur_proc = next_proc)
        {
            free(cur_proc->cmd);
            if (cur_proc->read_filepath)
            {
                free(cur_proc->read_filepath);
            }
            if (cur_proc->write_filepath)
            {
                free(cur_proc->write_filepath);
            }
            for (size_t i = 0; i < MAX_ARG_SIZE; i++)
            {
                if (cur_proc->args[i] == NULL)
                {
                    break;
                }
                free(cur_proc->args[i]);
            }

            next_proc = cur_proc->next;
            free(cur_proc);
        }

        free(cur_job->line);

        next_job = cur_job->next;
        free(cur_job);
    }
    shell->finished_jobs = cur_job; // initialize shell->finished_jobs with NULL
}

void wait_fore_job(Job *job)
{
    pid_t wpid;
    int status;
    Process *cur_proc;

    while (1)
    {
        if ((wpid = waitpid(-1, &status, WUNTRACED)) == -1)
        {
            if (errno != ECHILD)
                perror("waitpid");
            break; // break the loop if all child processes are terminated.
        }

        for (cur_proc = job->process_queue; cur_proc != NULL; cur_proc = cur_proc->next)
        {
            if (cur_proc->pid == wpid)
            {
                if (WIFEXITED(status))
                {
                    job->running_procs--;
                }
                else if (WIFSTOPPED(status))
                {
                    job->job_state = Stopped;
                    printf("[%d] Stopped %s\n", job->id, job->line);
                    return;
                }
                break;
            }
        }

        if (job->running_procs == 0)
        {
            job->job_state = Done;
            delete_job(job->id);
            append_finished_job(job);
            break;
        }
    }
}

void wait_back_job()
{
    pid_t wpid;
    int status;

    Job *cur_job = NULL, *next_job = NULL, *wait_job = NULL;
    Process *cur_proc = NULL;

    while (1)
    {
        if ((wpid = waitpid(-1, &status, WUNTRACED | WNOHANG)) == -1)
        {
            if (errno != ECHILD)
                perror("waitpid");
            break;
        }
        else if (wpid == 0)
        {
            return; // finish the loop if there is no process which has changed the state.
        }

        for (cur_job = shell->jobs; cur_job != NULL; cur_job = next_job)
        {
            next_job = cur_job->next; // set next_job here because if cur_job is finished, cur_job->next will be changed.
            for (cur_proc = cur_job->process_queue; cur_proc != NULL; cur_proc = cur_proc->next)
            {
                if (wpid == cur_proc->pid)
                {
                    wait_job = cur_job;
                    goto FINISH_SEARCH;
                }
            }
        }

    FINISH_SEARCH:
        if (WIFEXITED(status))
        {
            wait_job->running_procs--;
        }
        else if (WIFSTOPPED(status))
        {
            wait_job->job_state = Stopped;
            printf("[%d] Stopped %s\n", wait_job->id, wait_job->line);
        }

        if (wait_job->running_procs == 0)
        {
            wait_job->job_state = Done;
            delete_job(wait_job->id);
            append_finished_job(wait_job);
            printf("[%d] Done %s\n", wait_job->id, wait_job->line);
        }
    }
}

int main()
{
    // setvbuf(stdout, NULL, _IONBF, 0); // for test

    shell = calloc(1, sizeof(Shell));
    ignore_signal();

    while (1)
    {
        size_t line_size = 0;

        Token *head_token = (Token *)calloc(1, sizeof(Token));
        head_token->prev = NULL;

        printf("shellman$ ");
        line_size = getstdin(head_token);

        wait_back_job();

        shell->cur_job = new_job(line_size);
        append_job(shell->cur_job);

        parse(shell->cur_job, head_token);

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
        free_token(head_token);
        free_jobs(); // free jobs and finished_job_list
    }

    free(shell);
    exit(EXIT_SUCCESS);
}
