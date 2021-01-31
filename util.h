#ifndef util_h
#define util_h

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "signal.h"
#include <unistd.h>

static const char *builtins[] = {
    "jobs", "fg", "bg"};
static const size_t n_builtins = sizeof(builtins) / sizeof(char *);

void set_ignore();
void set_default();
void free_string(char *string);
bool is_builtin(char *cmd);

#endif
