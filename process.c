#include "process.h"

Process *new_process(Process *cur_process)
{
    Process *new_process = (Process *)calloc(1, sizeof(Process));
    if (new_process == NULL)
        return NULL;

    cur_process->next = new_process;
    return new_process;
}
