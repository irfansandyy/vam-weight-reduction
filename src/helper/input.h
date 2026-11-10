#ifndef VRB_INPUT_H
#define VRB_INPUT_H

#include "vrb.h"
#include <stdio.h>

/* Returns NULL on success, or a message on invalid input.
   Pass NULL for prompts to read without displaying questions. */
const char *vrb_read_problem(FILE *input, FILE *prompts, VrbProblem *problem);

#endif
