#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main()
{
    int c, i, position, count = 0;
    char buffer[100];
    char args[10][100]; // 改善の余地あり
    pid_t pid, wait_pid;

    while (1)
    {
        printf("> ");

        position = 0;
        count = 0;
        memset(buffer, '\0', 100);
        int status;

        while (1)
        {
            c = getchar();

            if (c == '\n')
            {
                strcpy(args[count], buffer);

                buffer[position] = '\0';
                c = 0;
                break;
            }
            else if (c == ' ')
            {
                strcpy(args[count], buffer);

                count++;

                position = 0;
                memset(buffer, '\0', 100);
                continue;
            }
            else if (c == EOF)
            {
                exit(0);
            }

            buffer[position] = c;
            position++;
        }

        // sleep command

        if ((pid = fork()) < 0)
        {
            perror("fork");
            exit(1);
        }
        else if (pid == 0)
        {
            // child process
            sleep(5);
            exit(0);
        }
        else
        {
            // parent process

            wait_pid = waitpid(pid, &status, WUNTRACED);
            if (wait_pid < 0)
            {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

            if (WIFEXITED(status))
            {
                printf("exited");
            }

            // while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }

        printf("\n");
    }

    exit(EXIT_SUCCESS);
}
