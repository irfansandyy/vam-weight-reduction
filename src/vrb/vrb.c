#include "vrb.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Each list is ordered once by reduced cost, then by opposite index.
   Retiring a row or column removes one entry from each opposite list. */
typedef struct {
    int first[VRB_MAX_DIMENSION], last[VRB_MAX_DIMENSION];
    int next[VRB_MAX_DIMENSION][VRB_MAX_DIMENSION];
    int previous[VRB_MAX_DIMENSION][VRB_MAX_DIMENSION];
    int minimum_count[VRB_MAX_DIMENSION];
    int active_count;
} ActiveLists;

typedef struct {
    int rows, columns;
    VrbMatrix reduced;
    VrbValue supply[VRB_MAX_DIMENSION], demand[VRB_MAX_DIMENSION];
    VrbValue row_reductions[VRB_MAX_DIMENSION], column_reductions[VRB_MAX_DIMENSION];
    VrbValue dummy_margin;
    ActiveLists row_lists, column_lists;
} State;

typedef struct {
    int index;
    VrbValue cost;
} Entry;

static VrbValue min_value(VrbValue a, VrbValue b)
{
    return a < b ? a : b;
}

static VrbValue original_cost(const VrbProblem *problem, int row, int column)
{
    return row < problem->rows && column < problem->columns
               ? problem->costs[row][column] : 0;
}

static VrbValue line_cost(const State *state, VrbAxis axis, int line, int position)
{
    return axis == VRB_ROW ? state->reduced[line][position] : state->reduced[position][line];
}

static VrbStatus prepare(const VrbProblem *problem, State *state, VrbResult *result)
{
    const int m = problem->rows, n = problem->columns;
    if (m < 1 || m > VRB_MAX_INPUT_DIMENSION || n < 1 || n > VRB_MAX_INPUT_DIMENSION)
        return VRB_INVALID_DIMENSIONS;

    VrbValue supply_total = 0, demand_total = 0;
    for (int i = 0; i < m; ++i) {
        const VrbValue supply = problem->supply[i];
        if (supply < 0) return VRB_NEGATIVE_INPUT;
        if (__builtin_add_overflow(supply_total, supply, &supply_total)) return VRB_OVERFLOW;
        state->supply[i] = supply;
    }
    for (int j = 0; j < n; ++j) {
        const VrbValue demand = problem->demand[j];
        if (demand < 0) return VRB_NEGATIVE_INPUT;
        if (__builtin_add_overflow(demand_total, demand, &demand_total)) return VRB_OVERFLOW;
        state->demand[j] = demand;
    }

    state->rows = m;
    state->columns = n;
    state->dummy_margin = 0;
    if (supply_total > demand_total) {
        state->dummy_margin = supply_total - demand_total;
        state->demand[state->columns++] = state->dummy_margin;
    } else if (demand_total > supply_total) {
        state->dummy_margin = demand_total - supply_total;
        state->supply[state->rows++] = state->dummy_margin;
    }
    result->rows = state->rows;
    result->columns = state->columns;
    result->iterations = 0;
    result->cost = 0;
    for (int i = 0; i < state->rows; ++i) {
        memset(result->allocation[i], 0, (size_t)state->columns * sizeof(VrbValue));
        for (int j = 0; j < state->columns; ++j) {
            const VrbValue value = original_cost(problem, i, j);
            if (value < 0) return VRB_NEGATIVE_INPUT;
            state->reduced[i][j] = value;
        }
    }

    /* Reduce only active cells. A line with zero remaining capacity is ignored. */
    for (int i = 0; i < state->rows; ++i) {
        state->row_reductions[i] = 0;
        if (state->supply[i] == 0) continue;
        VrbValue minimum = INT64_MAX;
        for (int j = 0; j < state->columns; ++j)
            if (state->demand[j] > 0) minimum = min_value(minimum, state->reduced[i][j]);
        state->row_reductions[i] = minimum;
        for (int j = 0; j < state->columns; ++j)
            if (state->demand[j] > 0) state->reduced[i][j] -= minimum;
    }
    for (int j = 0; j < state->columns; ++j) {
        state->column_reductions[j] = 0;
        if (state->demand[j] == 0) continue;
        VrbValue minimum = INT64_MAX;
        for (int i = 0; i < state->rows; ++i)
            if (state->supply[i] > 0) minimum = min_value(minimum, state->reduced[i][j]);
        state->column_reductions[j] = minimum;
        for (int i = 0; i < state->rows; ++i)
            if (state->supply[i] > 0) state->reduced[i][j] -= minimum;
    }
    return VRB_OK;
}

static int compare_entries(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    if (a->cost != b->cost) return a->cost < b->cost ? -1 : 1;
    return (a->index > b->index) - (a->index < b->index);
}

static void initialize_lists(State *state, VrbAxis axis)
{
    const bool column = axis == VRB_COLUMN;
    ActiveLists *lists = column ? &state->column_lists : &state->row_lists;
    const VrbValue *own = column ? state->demand : state->supply;
    const VrbValue *other = column ? state->supply : state->demand;
    const int own_count = column ? state->columns : state->rows;
    const int other_count = column ? state->rows : state->columns;
    lists->active_count = 0;
    for (int i = 0; i < own_count; ++i) {
        lists->first[i] = lists->last[i] = -1;
        lists->minimum_count[i] = 0;
        if (own[i] == 0) continue;
        ++lists->active_count;
        Entry order[VRB_MAX_DIMENSION];
        int count = 0;
        for (int j = 0; j < other_count; ++j)
            if (other[j] > 0) order[count++] = (Entry){j, line_cost(state, axis, i, j)};
        if (count == 0) continue;
        qsort(order, (size_t)count, sizeof order[0], compare_entries);
        lists->first[i] = order[0].index;
        lists->last[i] = order[count - 1].index;
        for (int j = 0; j < count; ++j) {
            const int index = order[j].index;
            lists->previous[i][index] = j == 0 ? -1 : order[j - 1].index;
            lists->next[i][index] = j + 1 == count ? -1 : order[j + 1].index;
            if (order[j].cost == order[0].cost) ++lists->minimum_count[i];
        }
    }
}

static void retire(State *state, VrbAxis axis, int index)
{
    const bool row = axis == VRB_ROW;
    ActiveLists *retired = row ? &state->row_lists : &state->column_lists;
    ActiveLists *remaining = row ? &state->column_lists : &state->row_lists;
    const VrbValue *other_margin = row ? state->demand : state->supply;
    const int other_count = row ? state->columns : state->rows;
    const VrbAxis other_axis = row ? VRB_COLUMN : VRB_ROW;
    --retired->active_count;
    for (int i = 0; i < other_count; ++i) {
        if (other_margin[i] == 0) continue;
        const int previous = remaining->previous[i][index];
        const int next = remaining->next[i][index];
        const VrbValue old_flc = line_cost(state, other_axis, i, remaining->first[i]);
        if (previous < 0) remaining->first[i] = next;
        else remaining->next[i][previous] = next;
        if (next < 0) remaining->last[i] = previous;
        else remaining->previous[i][next] = previous;

        if (line_cost(state, other_axis, i, index) == old_flc &&
            --remaining->minimum_count[i] == 0 && remaining->first[i] >= 0) {
            const int first = remaining->first[i];
            const VrbValue flc = line_cost(state, other_axis, i, first);
            for (int p = first; p >= 0 && line_cost(state, other_axis, i, p) == flc;
                 p = remaining->next[i][p])
                ++remaining->minimum_count[i];
        }
    }
}

static VrbStatus inspect_line(const State *state, VrbAxis axis, int index, VrbLine *line)
{
    const bool column = axis == VRB_COLUMN;
    const ActiveLists *lists = column ? &state->column_lists : &state->row_lists;
    const ActiveLists *other_lists = column ? &state->row_lists : &state->column_lists;
    const VrbValue own_quantity = column ? state->demand[index] : state->supply[index];
    const VrbValue *other = column ? state->supply : state->demand;
    const int first = lists->first[index];
    if (first < 0) return VRB_INFEASIBLE;
    const int *next = lists->next[index];
    const int second = next[first], third = second < 0 ? -1 : next[second];
    line->axis = axis;
    line->index = index;
    line->active_count = other_lists->active_count;
    line->flc_count = lists->minimum_count[index];
    line->flc = line_cost(state, axis, index, first);
    line->slc = second < 0 ? line->flc : line_cost(state, axis, index, second);
    line->tlc = third < 0 ? line->slc : line_cost(state, axis, index, third);
    line->max = line_cost(state, axis, index, lists->last[index]);

    int position = first;
    line->quantity = min_value(own_quantity, other[first]);
    int p = second;
    for (int count = 1; count < line->flc_count && line->quantity < own_quantity; ++count) {
        const VrbValue quantity = min_value(own_quantity, other[p]);
        if (quantity > line->quantity) {
            position = p;
            line->quantity = quantity;
        }
        p = next[p];
    }
    line->row = column ? position : index;
    line->column = column ? index : position;
    line->penalty = line->slc - line->flc;
    if (__builtin_add_overflow(line->max, line->tlc - line->flc, &line->weight) ||
        __builtin_mul_overflow(line->penalty, line->weight, &line->score))
        return VRB_OVERFLOW;
    return VRB_OK;
}

/* A positive value favors a and identifies the first criterion that differs. */
static int compare_lines(const State *state, const VrbLine *a, const VrbLine *b)
{
    if (a->score != b->score) return a->score > b->score ? VRB_SCORE : -VRB_SCORE;
    if (a->flc != b->flc) return a->flc < b->flc ? VRB_FLC : -VRB_FLC;
    const int a_row = state->row_lists.minimum_count[a->row];
    const int a_col = state->column_lists.minimum_count[a->column];
    const int b_row = state->row_lists.minimum_count[b->row];
    const int b_col = state->column_lists.minimum_count[b->column];
    const VrbValue a_min = min_value(a_row, a_col), b_min = min_value(b_row, b_col);
    if (a_min != b_min) return a_min < b_min ? VRB_MIN_COUNT : -VRB_MIN_COUNT;
    if (a_row + a_col != b_row + b_col)
        return a_row + a_col < b_row + b_col ? VRB_TOTAL_COUNT : -VRB_TOTAL_COUNT;
    if (a->quantity != b->quantity) return a->quantity > b->quantity ? VRB_QUANTITY : -VRB_QUANTITY;
    if (a->row != b->row) return a->row < b->row ? VRB_ROW_INDEX : -VRB_ROW_INDEX;
    if (a->column != b->column) return a->column < b->column ? VRB_COLUMN_INDEX : -VRB_COLUMN_INDEX;
    if (a->axis != b->axis) return a->axis < b->axis ? VRB_AXIS : -VRB_AXIS;
    if (a->index != b->index) return a->index < b->index ? VRB_LINE_INDEX : -VRB_LINE_INDEX;
    return 0;
}

static VrbStatus verify_result(const VrbProblem *problem, const State *state,
                               const VrbResult *result)
{
    VrbValue total = 0, column_sum[VRB_MAX_DIMENSION] = {0};
    for (int i = 0; i < result->rows; ++i) {
        VrbValue row_sum = 0;
        for (int j = 0; j < result->columns; ++j) {
            const VrbValue allocation = result->allocation[i][j];
            if (allocation < 0) return VRB_INFEASIBLE;
            VrbValue subtotal;
            if (__builtin_add_overflow(row_sum, allocation, &row_sum) ||
                __builtin_add_overflow(column_sum[j], allocation, &column_sum[j]) ||
                __builtin_mul_overflow(allocation, original_cost(problem, i, j), &subtotal) ||
                __builtin_add_overflow(total, subtotal, &total))
                return VRB_OVERFLOW;
        }
        const VrbValue expected = i < problem->rows ? problem->supply[i] : state->dummy_margin;
        if (row_sum != expected) return VRB_INFEASIBLE;
    }
    for (int j = 0; j < result->columns; ++j) {
        const VrbValue expected = j < problem->columns ? problem->demand[j] : state->dummy_margin;
        if (column_sum[j] != expected) return VRB_INFEASIBLE;
    }
    return total == result->cost ? VRB_OK : VRB_INFEASIBLE;
}

VrbStatus vrb_solve(const VrbProblem *problem, VrbResult *result, const VrbObserver *observer)
{
    State state;
    VrbStatus status = prepare(problem, &state, result);
    if (status != VRB_OK) return status;
    const VrbSetup setup = {
        problem, state.rows, state.columns, (const VrbMatrix *)&state.reduced,
        state.row_reductions, state.column_reductions, state.supply, state.demand
    };
    if (observer && observer->prepared) observer->prepared(observer->context, &setup);
    const bool track_steps = observer && observer->step;
    initialize_lists(&state, VRB_ROW);
    initialize_lists(&state, VRB_COLUMN);
    while (state.row_lists.active_count > 0) {
        VrbLine lines[VRB_MAX_DIMENSION * 2];
        int count = 0;
        for (int i = 0; i < state.rows; ++i) {
            if (state.supply[i] == 0) continue;
            status = inspect_line(&state, VRB_ROW, i, &lines[count++]);
            if (status != VRB_OK) return status;
        }
        for (int j = 0; j < state.columns; ++j) {
            if (state.demand[j] == 0) continue;
            status = inspect_line(&state, VRB_COLUMN, j, &lines[count++]);
            if (status != VRB_OK) return status;
        }
        int best = 0;
        for (int i = 1; i < count; ++i)
            if (compare_lines(&state, &lines[i], &lines[best]) > 0) best = i;
        if (count == 0 || lines[best].quantity <= 0) return VRB_INFEASIBLE;
        const VrbLine *chosen = &lines[best];
        const int row = chosen->row, column = chosen->column;

        VrbValue supply_before[VRB_MAX_DIMENSION], demand_before[VRB_MAX_DIMENSION];
        int alternative = -1;
        if (track_steps) {
            memcpy(supply_before, state.supply, (size_t)state.rows * sizeof(VrbValue));
            memcpy(demand_before, state.demand, (size_t)state.columns * sizeof(VrbValue));
            /* A candidate proposed by both axes still describes a single cell. */
            for (int i = 0; i < count; ++i) {
                if (lines[i].row == row && lines[i].column == column) continue;
                if (alternative < 0 || compare_lines(&state, &lines[i], &lines[alternative]) > 0)
                    alternative = i;
            }
        }
        VrbValue step_cost;
        if (__builtin_mul_overflow(chosen->quantity, original_cost(problem, row, column), &step_cost) ||
            __builtin_add_overflow(result->cost, step_cost, &result->cost) ||
            __builtin_add_overflow(result->allocation[row][column], chosen->quantity,
                                   &result->allocation[row][column]))
            return VRB_OVERFLOW;
        state.supply[row] -= chosen->quantity;
        state.demand[column] -= chosen->quantity;
        ++result->iterations;
        if (track_steps) {
            const VrbChoice reason = alternative < 0 ? VRB_ONLY_CELL
                                      : (VrbChoice)compare_lines(&state, chosen, &lines[alternative]);
            const VrbStep step = {
                result->iterations, lines, count, best, alternative, reason,
                state.row_lists.minimum_count, state.column_lists.minimum_count,
                supply_before, demand_before, state.supply, state.demand, step_cost, result->cost
            };
            observer->step(observer->context, &setup, &step);
        }
        if (state.supply[row] == 0) retire(&state, VRB_ROW, row);
        if (state.demand[column] == 0) retire(&state, VRB_COLUMN, column);
    }
    return verify_result(problem, &state, result);
}
