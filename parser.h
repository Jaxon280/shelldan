#ifndef parser_h
#define parser_h

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "job.h"
#include "process.h"
#include "util.h"

#define MAX_BUFFER_SIZE 256

typedef enum tokenlabel
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

typedef struct token
{
    TokenLabel label;
    struct token *prev;
    struct token *next;
    char *string;
    int size;
    int arg_order;
} Token;

Token *new_token(Token *cur_token);
size_t tokenize(Token *token, char *buffer);
size_t tokenize_line(Token *token);
char *copy_token_string(char *dest, Token *token);
// If failed to parse token, return -1 instead of 0
int8_t parse(Job *job, Token *head_token);
void free_token(Token *token);

#endif
