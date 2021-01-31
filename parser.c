#include "parser.h"

Token *new_token(Token *cur_token)
{
    Token *new_token = (Token *)calloc(1, sizeof(Token));
    if (new_token == NULL)
        return NULL;

    cur_token->next = new_token;
    new_token->prev = cur_token;
    return new_token;
}

size_t tokenize(Token *token, char *buffer)
{
    if (token->prev == NULL || token->prev->label == PIPE)
    {
        token->arg_order = 0;
        if (is_builtin(buffer) == true)
            token->label = BUILTIN_CMD;
        else
            token->label = CMD;
    }
    else if (strcmp(buffer, "|") == 0)
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
    else
    {
        token->arg_order = token->prev->arg_order + 1;
        token->label = ARG;
    }

    size_t token_size = (strlen(buffer) + 1) * sizeof(char);
    token->string = (char *)malloc(token_size);

    strcpy(token->string, buffer);
    token->size = token_size;

    return token_size + 1;
}

size_t tokenize_line(Token *token)
{
    int c;
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, '\0', sizeof(buffer));
    size_t line_size = 0;

    while ((c = getchar()) != EOF)
    {
        if (token == NULL)
            break;

        if (c == ' ')
        {
            if (strlen(buffer) > 0)
            {
                line_size += tokenize(token, buffer);
                token = new_token(token);
                memset(buffer, '\0', sizeof(buffer));
            }
            else
            {
                continue;
            }
        }
        else if (c == '\n')
        {
            if (strlen(buffer) > 0)
            {
                line_size += tokenize(token, buffer);
                token = new_token(token);
                memset(buffer, '\0', sizeof(buffer));
            }

            token->label = NONE;
            break;
        }
        else
        {
            if (strlen(buffer) >= MAX_BUFFER_SIZE)
            {
                printf("-shellman: command too long\n");
                return 0;
            }

            buffer[strlen(buffer)] = c;
        }
    }

    if (c == EOF)
    {
        printf("-shellman: scanning EOF terminates shellman.\n");
        exit(EXIT_SUCCESS);
    }

    return line_size;
}

char *copy_token_string(char *dest, Token *token)
{
    if (token->string == NULL)
        return NULL;

    dest = (char *)malloc(token->size);
    if (dest == NULL)
        return NULL;

    strcpy(dest, token->string);
    return dest;
}

// If failed to parse token, return -1 instead of 0
int8_t parse(Job *job, Token *head_token)
{
    Token *cur_token;
    Process *cur_process = job->process_queue;

    for (cur_token = head_token; cur_token->label != NONE; cur_token = cur_token->next)
    {
        if (cur_process == NULL) // if memory allocation is failed
            return -1;

        switch (cur_token->label)
        {
        case CMD: // <CMD> ( <ARG> <ARG> ... )
            cur_process->cmd = copy_token_string(cur_process->cmd, cur_token);
            break;

        case BUILTIN_CMD:
            cur_process->cmd = copy_token_string(cur_process->cmd, cur_token);
            job->job_mode = BUILTIN_MODE;
            break;

        case ARG:
            if (cur_token->arg_order - 1 >= MAX_ARG_SIZE)
            {
                printf("-shellman: a command's arguments are limited up to 8\n");
                return -1;
            }

            cur_process->args[cur_token->arg_order - 1] = copy_token_string(cur_process->args[cur_token->arg_order - 1], cur_token);
            break;

        case FILE_PATH:
            if (cur_token->prev->label == LEFT_REDIRECT)
                cur_process->read_filepath = copy_token_string(cur_process->read_filepath, cur_token);
            else if (cur_token->prev->label == RIGHT_REDIRECT)
                cur_process->write_filepath = copy_token_string(cur_process->write_filepath, cur_token);

            break;

        case PIPE: // <CMD> ... ( "<" or ">" <FILE> ) "|" <CMD> ...
            if (cur_token->next->label == NONE)
            {
                printf("-shellman: no command after pipe('|').\n");
                return -1;
            }

            cur_process = new_process(cur_process); // new_process() is invoked only here, because new CMD is guaranteed to come just after PIPE.
            break;

        case LEFT_REDIRECT:
            if (cur_token->next->label == NONE)
            {
                printf("-shellman: no filepath after redirect.\n");
                return -1;
            }
            break;

        case RIGHT_REDIRECT:
            if (cur_token->next->label == NONE)
            {
                printf("-shellman: no filepath after redirect.\n");
                return -1;
            }
            break;

        case BACKGROUND: // <CMD> ... "&"\n <--- "&" has to come to the end of input.
            if (cur_token->next->label != NONE)
            {
                printf("-shellman: '&' operator should come to the end of command.\n");
                return -1;
            }

            job->job_mode = BACK_MODE;
            break;

        default:
            break;
        }

        if (job->line == NULL)
            continue;
        strcat(job->line, cur_token->string);
        if (cur_token->next->label != NONE)
            strcat(job->line, " ");
    }

    cur_process->next = NULL; // set dummy node
    return 0;
}

void free_token(Token *token)
{
    Token *cur_token, *next_token;
    for (cur_token = token; cur_token != NULL; cur_token = next_token)
    {
        free_string(cur_token->string);

        next_token = cur_token->next;
        free(cur_token);
        cur_token = NULL;
    }
}
