#ifndef VRB_TRACE_H
#define VRB_TRACE_H

#include "vrb.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    FILE *output;
    bool json, steps;
} VrbOutput;

VrbObserver vrb_output_observer(VrbOutput *output);
void vrb_print_result(const VrbOutput *output, const VrbResult *result);

#endif
