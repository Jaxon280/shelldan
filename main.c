#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct file_process
{
    pid_t pid;
    FILE *fp;
    char file_path[100];
    int read_pipe;
    int write_pipe;
};
typedef struct file_process FileProcess;

struct process
{
    pid_t pid;
    char cmd[100];
    char *args[4];
    int read_pipe;
    int write_pipe;
};
typedef struct process Process;

enum JobState
{
    Pending,
    Running,
    Stopped,
    Done
};

struct job
{
    int id;
    enum JobState state;

    FileProcess fread_process;
    int is_fread;
    FileProcess fwrite_process;
    int is_fwrite;

    Process exec_process[10];
    int n_process; // the total number of ( fwrite_process / fread_process / exec_processes )
};
typedef struct job Job;

typedef enum
{
    NONE,
    PIPE,           // <CMD> ... ( "<" or ">" <FILE> ) "|" <CMD> ...
    LEFT_REDIRECT,  // <CMD> ... "<" <FILE(FREAD)>
    RIGHT_REDIRECT, // <CMD> ... ">" <FILE(FWRITE)>
    BACKGROUND,     // <CMD> ... "&"\n <--- "&" has to come to the end of input.
    CMD,            // <CMD> ( <ARG> <ARG> ... )
    ARG,
    FILE_PATH
} WordCategory;

#define DEFAULT_JOB_SIZE 4
#define MAX_WORD_SIZE 20
#define MAX_CH_SIZE 100

void parse_command()
{
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0); // for test

    printf("Launched shell\n");

    Job job_queue[DEFAULT_JOB_SIZE]; // Grab memory for Job queue (default_job_size)
    for (int i = 0; i < DEFAULT_JOB_SIZE; i++)
    {
        job_queue[i] = (Job){0};
        job_queue[i].id = i; // memo: ジョブの ID は Pending でないもののなかで設定するようにしておく？
    }

    int job_id = 0;
    int c;

    while (1)
    {
        job_id++;

        printf("> ");

        int char_count = 0, word_count = 0, cmd_count = 0, redirect_flag = 0;
        // "cmd_count" includes CMD and ARG.

        char buffer[MAX_WORD_SIZE][MAX_CH_SIZE];
        memset(buffer, '\0', MAX_WORD_SIZE * MAX_CH_SIZE);

        WordCategory word_mark[MAX_WORD_SIZE];
        memset(word_mark, NONE, MAX_WORD_SIZE * sizeof(WordCategory));

        while (1)
        {
            c = getchar();

            if (c == ' ' && char_count > 0)
            {
                // marking operator
                if (strcmp(buffer[word_count], "|") == 0) // <CMD> ... ( "<" or ">" <FILE> ) "|" <CMD> ...
                {
                    word_mark[word_count] = PIPE;
                    cmd_count = 0;
                }
                else if (strcmp(buffer[word_count], "<") == 0) // <CMD> ... "<" <FILE(FREAD)>
                {
                    word_mark[word_count] = LEFT_REDIRECT;
                    cmd_count = 0;
                    redirect_flag = 1;
                }
                else if (strcmp(buffer[word_count], ">") == 0) // <CMD> ... ">" <FILE(FWRITE)>
                {
                    word_mark[word_count] = RIGHT_REDIRECT;
                    cmd_count = 0;
                    redirect_flag = 1;
                }
                else if (strcmp(buffer[word_count], "&") == 0) // <CMD> ... "&"\n <--- "&" has to come to the end of input.
                {
                    word_mark[word_count] = BACKGROUND;
                    cmd_count = 0;
                }
                else if (redirect_flag == 1)
                {
                    word_mark[word_count] = FILE_PATH;
                    redirect_flag = 0;
                }
                else if (cmd_count == 0) // <CMD> ( <ARG> <ARG> ... )
                {
                    word_mark[word_count] = CMD;
                    cmd_count++;
                }
                else
                {
                    word_mark[word_count] = ARG;
                    cmd_count++;
                }

                word_count++;
                char_count = 0;
            }
            else if (c == '\n')
            {
                if (char_count > 0)
                {
                    if (redirect_flag == 1)
                    {
                        word_mark[word_count] = FILE_PATH;
                        redirect_flag = 0;
                    }
                    else if (cmd_count == 0)
                    {
                        word_mark[word_count] = CMD;
                        cmd_count++;
                    }
                    else
                    {
                        word_mark[word_count] = ARG;
                        cmd_count++;
                    }

                    word_count++;
                    char_count = 0;
                }
                break;
            }
            else
            {
                buffer[word_count][char_count] = c;
                char_count++;
            }

            if (c == EOF)
            {
                exit(0);
            }
        }

        /* Job object is initialized here */
        Job job;
        job = (Job){0};
        job.id = job_id;

        int pipefd[MAX_WORD_SIZE][2];

        int eprocess_count = 0, arg_count = 0, fread_flag = 0, fwrite_flag = 0, pipe_count = 0;

        for (size_t i = 0; i < word_count; i++)
        {
            // if a word is operator
            switch (word_mark[i])
            {
            case CMD: // <CMD> ( <ARG> <ARG> ... )
                job.n_process++;
                strcpy(job.exec_process[eprocess_count].cmd, buffer[i]);

                arg_count = 0;
                break;

            case ARG:
                strcpy(job.exec_process[eprocess_count].args[arg_count], buffer[i]);

                arg_count++;
                break;

            case FILE_PATH:
                if (fread_flag)
                {
                    strcpy(job.fread_process.file_path, buffer[i]);

                    fread_flag--;
                }
                else if (fwrite_flag)
                {
                    strcpy(job.fwrite_process.file_path, buffer[i]);

                    fwrite_flag--;
                }
                break;

            case PIPE: // <CMD> ... ( "<" or ">" <FILE> ) "|" <CMD> ...
                pipe(pipefd[pipe_count]);
                job.exec_process[eprocess_count].write_pipe = pipefd[pipe_count][1];
                job.exec_process[eprocess_count + 1].read_pipe = pipefd[pipe_count][0]; // set read pipe for next process

                eprocess_count++; // eprocess_count is incremented only here, because new CMD is guaranteed to come just after PIPE.
                pipe_count++;
                break;

            case LEFT_REDIRECT: // <CMD> ... "<" <FILE(FREAD)>
                pipe(pipefd[pipe_count]);
                job.exec_process[eprocess_count].read_pipe = pipefd[pipe_count][0];
                job.fread_process.write_pipe = pipefd[pipe_count][1];
                job.is_fread++;

                pipe_count++;
                fread_flag++;
                break;

            case RIGHT_REDIRECT: // <CMD> ... ">" <FILE(FWRITE)>
                pipe(pipefd[pipe_count]);
                job.exec_process[eprocess_count].write_pipe = pipefd[pipe_count][1];
                job.fwrite_process.read_pipe = pipefd[pipe_count][0];
                job.is_fwrite++;

                pipe_count++;
                fwrite_flag++;
                break;

            case BACKGROUND: // <CMD> ... "&"\n <--- "&" has to come to the end of input.

            default:
                break;
            }
        }

        job.state = Pending;

        pid_t pid, wait_pid;
        int status;

        if (job.is_fread == 1)
        {
            switch (pid = fork())
            {
            case -1:
                perror("fork fread_process");
                break;

            case 0:
                if ((job.fread_process.fp = fopen(job.fread_process.file_path, "r")) == NULL)
                {
                    perror("open in fread_process");
                    _exit(EXIT_FAILURE);
                }
                else
                {
                    char buffer;
                    while (fscanf(job.fread_process.fp, "%c", &buffer) == 1)
                    {
                        write(job.fread_process.write_pipe, &buffer, 1);
                    }

                    if (close(job.fread_process.write_pipe) == -1)
                    {
                        perror("close write_pipe in fread_process");
                        _exit(EXIT_FAILURE);
                    }

                    if (fclose(job.fread_process.fp) == -1)
                    {
                        perror("close fp in fread_process");
                        _exit(EXIT_FAILURE);
                    }
                }

                break;

            default:
                job.fread_process.pid = pid;
                wait_pid = waitpid(pid, &status, WUNTRACED);
                if (wait_pid == -1)
                {
                    perror("waitpid in fread_process");
                    exit(EXIT_FAILURE);
                }

                if (WIFEXITED(status))
                {
                    printf("exited in fread_process\n");
                }

                break;
            }
        }

        for (size_t i = 0; i < job.n_process; i++)
        {
            switch (pid = fork())
            {
            case -1:
                perror("fork child process");
                break;

            case 0:
                if (job.exec_process[i].write_pipe)
                {
                    dup2(job.exec_process[i].write_pipe, STDOUT_FILENO);
                    close(job.exec_process[i].write_pipe);
                }
                if (job.exec_process[i].read_pipe)
                {
                    dup2(job.exec_process[i].read_pipe, STDIN_FILENO);
                    close(job.exec_process[i].read_pipe);
                }

                if (execv(job.exec_process[i].cmd, job.exec_process[i].args) == -1)
                {
                    perror("exec child process");
                    _exit(EXIT_FAILURE);
                }
                break;

            default:
                job.exec_process[i].pid = pid;
                wait_pid = waitpid(pid, &status, WUNTRACED);
                if (wait_pid == -1)
                {
                    perror("waitpid child process");
                    exit(EXIT_FAILURE);
                }

                if (WIFEXITED(status))
                {
                    printf("exited %zu th of child process\n", i);
                }

                break;
            }
        }

        if (job.is_fwrite == 1)
        {
            switch (pid = fork())
            {
            case -1:
                perror("fork fwrite_process");
                break;

            case 0:
                if ((job.fwrite_process.fp = fopen(job.fwrite_process.file_path, "w")) == NULL)
                {
                    perror("open in fwrite_process");
                    _exit(EXIT_FAILURE);
                }
                else
                {
                    char buffer[100];
                    read(job.fwrite_process.read_pipe, buffer, sizeof(buffer));
                    fprintf(job.fwrite_process.fp, "%s", buffer);

                    if (close(job.fwrite_process.read_pipe) == -1)
                    {
                        perror("close read_pipe in fwrite_process");
                        _exit(EXIT_FAILURE);
                    }

                    if (fclose(job.fwrite_process.fp) == -1)
                    {
                        perror("close fp in fwrite_process");
                        _exit(EXIT_FAILURE);
                    }
                }

                break;

            default:
                job.fwrite_process.pid = pid;
                wait_pid = waitpid(pid, &status, WUNTRACED);
                if (wait_pid == -1)
                {
                    perror("waitpid in fwrite_process");
                    exit(EXIT_FAILURE);
                }

                if (WIFEXITED(status))
                {
                    printf("exited in fwrite_process\n");
                }

                break;
            }
        }
        printf("\n");
    }

    // release memory here.

    exit(EXIT_SUCCESS);
}
