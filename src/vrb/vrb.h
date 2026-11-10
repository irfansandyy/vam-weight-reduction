#ifndef VRB_H
#define VRB_H

#include <stdint.h>

enum { VRB_MAX_DIMENSION = 128, VRB_MAX_INPUT_DIMENSION = 127 };
typedef int64_t VrbValue;
typedef VrbValue VrbMatrix[VRB_MAX_DIMENSION][VRB_MAX_DIMENSION];

typedef struct {
    int rows, columns;
    VrbMatrix costs;
    VrbValue supply[VRB_MAX_DIMENSION], demand[VRB_MAX_DIMENSION];
} VrbProblem;

typedef struct {
    int rows, columns, iterations;
    VrbValue cost;
    VrbMatrix allocation;
} VrbResult;

typedef enum {
    VRB_OK,
    VRB_INVALID_DIMENSIONS,
    VRB_NEGATIVE_INPUT,
    VRB_OVERFLOW,
    VRB_INFEASIBLE
} VrbStatus;

typedef enum { VRB_ROW, VRB_COLUMN } VrbAxis;

typedef struct {
    VrbAxis axis;
    int index, active_count, flc_count, row, column;
    VrbValue flc, slc, tlc, max, penalty, weight, score, quantity;
} VrbLine;

typedef enum {
    VRB_ONLY_CELL,
    VRB_SCORE,
    VRB_FLC,
    VRB_MIN_COUNT,
    VRB_TOTAL_COUNT,
    VRB_QUANTITY,
    VRB_ROW_INDEX,
    VRB_COLUMN_INDEX,
    VRB_AXIS,
    VRB_LINE_INDEX
} VrbChoice;

typedef struct {
    const VrbProblem *problem;
    int rows, columns;
    const VrbMatrix *reduced;
    const VrbValue *row_reductions, *column_reductions;
    const VrbValue *supply, *demand;
} VrbSetup;

typedef struct {
    int iteration;
    const VrbLine *lines;
    int line_count, selected, alternative;
    VrbChoice reason;
    const int *row_counts, *column_counts;
    const VrbValue *supply_before, *demand_before;
    const VrbValue *supply_after, *demand_after;
    VrbValue step_cost, cumulative_cost;
} VrbStep;

/* Views and their pointers are valid only during the callback. Indices are zero-based. */
typedef struct {
    void *context;
    void (*prepared)(void *, const VrbSetup *);
    void (*step)(void *, const VrbSetup *, const VrbStep *);
} VrbObserver;

/* The input is not modified. The observer is optional. */
VrbStatus vrb_solve(const VrbProblem *problem, VrbResult *result,
                    const VrbObserver *observer);

#endif
