#include "job.h"

int set_jobid()
{
    Job *cur_job;
    int max = 0;
    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (max < cur_job->id)
        {
            max = cur_job->id;
        }
    }

    return max + 1;
}

Job *new_job(size_t byte_size)
{
    Job *new_job = (Job *)calloc(1, sizeof(Job));
    if (new_job == NULL)
        return NULL;

    new_job->id = set_jobid();
    new_job->job_mode = FORE_MODE;
    new_job->job_state = Pending;
    new_job->line = (char *)calloc(byte_size, sizeof(char));
    new_job->running_procs = 0;
    new_job->process_queue = (Process *)calloc(1, sizeof(Process));

    return new_job;
}

void insert_job(Job *new_job)
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

void insert_finished_job(Job *finished_job)
{
    if (finished_job->next != NULL)
    {
        printf("-shellman: internal error\n");
        exit(1);
    }

    finished_job->next = shell->finished_jobs;
    shell->finished_jobs = finished_job;
}

/* builtin commands */

void jobs(char **args)
{
    Job *cur_job;
    char state[8];

    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
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

        case Killed:
            strcpy(state, "Killed");
            break;

        default:
            break;
        }

        printf("[%d] %s %s\n", cur_job->id, state, cur_job->line);
    }
}

void fg(char **args)
{
    if (args == NULL)
    {
        printf("-shellman: fg example usage: `fg <job-id>`\n");
        return;
    }

    Job *cur_job;
    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (cur_job->id == atoi(args[0]) && cur_job->job_state == Stopped)
        {
            shell->cur_job = cur_job;
            shell->cur_job->job_mode = FORE_MODE;
            kill(-shell->cur_job->pgid, SIGCONT);
            if (tcsetpgrp(STDIN_FILENO, shell->cur_job->pgid) == -1)
            {
                perror("tcsetpgrp");
            }

            printf("fg [%d] %s\n", shell->cur_job->id, shell->cur_job->line);
            return;
        }
    }

    printf("-shellman: no job id: %d.\n", atoi(args[0]));
}

void bg(char **args)
{
    if (args == NULL)
    {
        printf("-shellman: bg example usage: `bg <job-id>`\n");
        return;
    }

    Job *cur_job;
    for (cur_job = shell->jobs; cur_job != NULL; cur_job = cur_job->next)
    {
        if (cur_job->id == atoi(args[0]) && cur_job->job_state == Stopped)
        {
            shell->cur_job = cur_job;
            shell->cur_job->job_mode = BACK_MODE;
            kill(-shell->cur_job->pgid, SIGCONT);
            printf("bg [%d] %s\n", shell->cur_job->id, shell->cur_job->line);
            return;
        }
    }

    printf("-shellman: no job id: %d.\n", atoi(args[0]));
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

/* builtin commands end here. */

void run_job(Job *job)
{
    pid_t pid;

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
                perror("-shellman: open");
                printf("-shellman: filepath is :%s", process->read_filepath);
                continue;
            }
            process->read_fd = read_fd;
        }

        if (process->write_filepath != NULL)
        {
            if (write_fd != 0)
                close(write_fd);

            if ((write_fd = open(process->write_filepath, O_WRONLY | O_TRUNC | O_CREAT)) == -1)
            {
                perror("-shellman: open\n");
                printf("-shellman: filepath is :%s", process->write_filepath);
                continue;
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

        pid = fork();

        if (pid == -1)
        {
            perror("-shellman: fork\n");
        }
        else if (pid == 0)
        {
            set_default();

            if (process->read_fd)
            {
                if (dup2(process->read_fd, STDIN_FILENO) == -1)
                {
                    perror("-shellman: dup2\n");
                    exit(1);
                }
                if (close(process->read_fd) == -1)
                {
                    perror("-shellman: close\n");
                    exit(1);
                }
            }

            if (process->write_fd)
            {
                if (dup2(process->write_fd, STDOUT_FILENO) == -1)
                {
                    perror("-shellman: dup2\n");
                    exit(1);
                }
                if (close(process->write_fd) == -1)
                {
                    perror("-shellman: close\n");
                    exit(1);
                }
            }

            if (execv(process->cmd, process->args) == -1)
            {
                perror("-shellman: exec");
                exit(1); // Exited with error
            }
            exit(0);
        }
        else
        {
            if (process->read_fd)
            {
                if (close(process->read_fd) == -1)
                {
                    perror("-shellman: close\n");
                    break;
                }
            }
            if (process->write_fd)
            {
                if (close(process->write_fd) == -1)
                {
                    perror("-shellman: close\n");
                    break;
                }
            }

            process->pid = pid;
            if (!job->pgid)
            {
                if (setpgid(pid, pid) == -1)
                {
                    perror("-shellman: setpgid\n");
                    break;
                }
                job->pgid = pid;
            }
            else
            {
                if (setpgid(pid, job->pgid) == -1)
                {
                    perror("-shellman: setpgid\n");
                    break;
                }
            }

            job->running_procs++;

            if (job->job_mode == FORE_MODE && job->pgid == pid)
            {
                if (tcsetpgrp(STDIN_FILENO, shell->cur_job->pgid) == -1)
                {
                    perror("-shellman: tcsetpgrp\n");
                    break;
                }
            }
        }
    }
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
                perror("-shellman: waitpid");
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
                else if (WTERMSIG(status) == SIGINT)
                {
                    job->running_procs--;
                }
                else if (WTERMSIG(status) == SIGKILL || WTERMSIG(status) == SIGTERM)
                {
                    if (job->job_state == Killed)
                    {
                        kill(-job->pgid, SIGKILL);
                        job->job_state = Killed;
                    }

                    job->running_procs--;
                }
                break;
            }
        }

        if (job->running_procs == 0)
        {
            if (job->job_state != Killed)
            {
                job->job_state = Done;
            }
            delete_job(job->id);
            insert_finished_job(job);
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
                perror("-shellman: waitpid");
            break; // break the loop if all child processes are terminated.
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
            if (wait_job->job_state == Stopped)
                continue;

            wait_job->job_state = Stopped;
            printf("[%d] Stopped %s\n", wait_job->id, wait_job->line);
        }
        else if (WTERMSIG(status) == SIGINT)
        {
            wait_job->running_procs--;
        }
        else if (WTERMSIG(status) == SIGKILL || WTERMSIG(status) == SIGTERM)
        {
            kill(-wait_job->pgid, SIGKILL);
            wait_job->job_state = Killed;
            wait_job->running_procs--;
        }

        if (wait_job->running_procs == 0)
        {
            if (wait_job->job_state != Killed)
            {
                wait_job->job_state = Done;
            }
            delete_job(wait_job->id);
            insert_finished_job(wait_job);
            printf("[%d] Done %s\n", wait_job->id, wait_job->line);
        }
    }
}

void free_jobs()
{
    Job *cur_job, *next_job = NULL;
    Process *cur_proc, *next_proc = NULL;
    for (cur_job = shell->finished_jobs; cur_job != NULL; cur_job = next_job)
    {
        for (cur_proc = cur_job->process_queue; cur_proc != NULL; cur_proc = next_proc)
        {
            free_string(cur_proc->cmd);
            free_string(cur_proc->read_filepath);
            free_string(cur_proc->write_filepath);
            for (size_t i = 0; i < MAX_ARG_SIZE; i++)
            {
                if (cur_proc->args[i] == NULL)
                {
                    break;
                }
                free_string(cur_proc->args[i]);
            }

            next_proc = cur_proc->next;
            free(cur_proc);
            cur_proc = NULL;
        }

        free_string(cur_job->line);

        next_job = cur_job->next;
        free(cur_job);
        cur_job = NULL;
    }
    shell->finished_jobs = cur_job; // initialize finished_jobs with NULL
}
