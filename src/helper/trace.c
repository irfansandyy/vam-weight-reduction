#include "trace.h"

#include <inttypes.h>

static void print_vector(FILE *out, const VrbValue *values, int count, bool json)
{
    if (json) fputc('[', out);
    for (int i = 0; i < count; ++i) {
        if (i > 0) fputc(json ? ',' : ' ', out);
        fprintf(out, "%" PRId64, values[i]);
    }
    fputc(json ? ']' : '\n', out);
}

static void print_matrix(FILE *out, const VrbMatrix *matrix, int rows, int columns, bool json)
{
    if (json) fputc('[', out);
    for (int i = 0; i < rows; ++i) {
        if (json && i > 0) fputc(',', out);
        print_vector(out, (*matrix)[i], columns, json);
    }
    if (json) fputc(']', out);
}

static void prepared(void *context, const VrbSetup *setup)
{
    const VrbOutput *output = context;
    FILE *out = output->output;
    if (output->json) {
        fprintf(out, "{\"method\":\"VRB_Scarcity\",\"rows\":%d,\"columns\":%d,"
                     "\"original_rows\":%d,\"original_columns\":%d,\"row_reductions\":",
                setup->rows, setup->columns, setup->problem->rows, setup->problem->columns);
        print_vector(out, setup->row_reductions, setup->rows, true);
        fputs(",\"column_reductions\":", out);
        print_vector(out, setup->column_reductions, setup->columns, true);
        fputs(",\"reduced_costs\":", out);
        print_matrix(out, setup->reduced, setup->rows, setup->columns, true);
        if (output->steps) fputs(",\"trace\":[", out);
        return;
    }
    fputs("Persiapan VRB\n", out);
    fprintf(out, "Ukuran input: %d x %d\n", setup->problem->rows, setup->problem->columns);
    if (setup->rows > setup->problem->rows) fputs("Ditambah satu baris dummy berbiaya nol.\n", out);
    if (setup->columns > setup->problem->columns) fputs("Ditambah satu kolom dummy berbiaya nol.\n", out);
    fputs("Minimum baris: ", out);
    print_vector(out, setup->row_reductions, setup->rows, false);
    fputs("Minimum kolom setelah reduksi baris: ", out);
    print_vector(out, setup->column_reductions, setup->columns, false);
    fputs("Reduksi hanya sekali. Biaya akhir memakai matriks asli.\n", out);
}

static void json_line(FILE *out, const VrbLine *line, const VrbStep *step)
{
    fprintf(out, "{\"axis\":\"%s\",\"index\":%d,\"active_count\":%d,"
                 "\"FLC\":%" PRId64 ",\"SLC\":%" PRId64 ",\"TLC\":%" PRId64
                 ",\"MAX\":%" PRId64 ",\"penalty\":%" PRId64
                 ",\"weight\":%" PRId64 ",\"score\":%" PRId64
                 ",\"row\":%d,\"column\":%d,\"quantity\":%" PRId64
                 ",\"FLC_ROW_COUNT\":%d,\"FLC_COLUMN_COUNT\":%d}",
            line->axis == VRB_COLUMN ? "column" : "row", line->index + 1, line->active_count,
            line->flc, line->slc, line->tlc, line->max, line->penalty, line->weight, line->score,
            line->row + 1, line->column + 1, line->quantity,
            step->row_counts[line->row], step->column_counts[line->column]);
}

static void json_step(FILE *out, const VrbSetup *setup, const VrbStep *step)
{
    fprintf(out, "%s{\"iteration\":%d,\"supply_before\":", step->iteration > 1 ? "," : "", step->iteration);
    print_vector(out, step->supply_before, setup->rows, true);
    fputs(",\"demand_before\":", out);
    print_vector(out, step->demand_before, setup->columns, true);
    fputs(",\"lines\":[", out);
    for (int i = 0; i < step->line_count; ++i) {
        if (i > 0) fputc(',', out);
        json_line(out, &step->lines[i], step);
    }
    fputs("],\"selected\":", out);
    json_line(out, &step->lines[step->selected], step);
    fputs(",\"supply_after\":", out);
    print_vector(out, step->supply_after, setup->rows, true);
    fputs(",\"demand_after\":", out);
    print_vector(out, step->demand_after, setup->columns, true);
    fprintf(out, ",\"step_cost\":%" PRId64 ",\"cumulative_cost\":%" PRId64 "}", step->step_cost, step->cumulative_cost);
}

static int minimum_count(const VrbStep *step, const VrbLine *line)
{
    const int rows = step->row_counts[line->row], columns = step->column_counts[line->column];
    return rows < columns ? rows : columns;
}

static int total_count(const VrbStep *step, const VrbLine *line)
{
    return step->row_counts[line->row] + step->column_counts[line->column];
}

static void print_active_costs(FILE *out, const VrbSetup *setup, const VrbStep *step)
{
    fputs("Biaya tereduksi pada baris dan kolom aktif:\n", out);
    int width = 4;
    for (int i = 0; i < setup->rows; ++i) {
        if (step->supply_before[i] == 0) continue;
        for (int j = 0; j < setup->columns; ++j) {
            if (step->demand_before[j] == 0) continue;
            int digits = snprintf(NULL, 0, "%" PRId64, (*setup->reduced)[i][j]);
            if (digits > width) width = digits;
        }
    }
    fprintf(out, "%6s", "");
    for (int j = 0; j < setup->columns; ++j) {
        if (step->demand_before[j] == 0) continue;
        char label[16];
        snprintf(label, sizeof label, "K%d", j + 1);
        fprintf(out, " %*s", width, label);
    }
    fputc('\n', out);
    for (int i = 0; i < setup->rows; ++i) {
        if (step->supply_before[i] == 0) continue;
        fprintf(out, "B%-5d", i + 1);
        for (int j = 0; j < setup->columns; ++j)
            if (step->demand_before[j] > 0) fprintf(out, " %*" PRId64, width, (*setup->reduced)[i][j]);
        fputc('\n', out);
    }
}

static void print_candidates(FILE *out, const VrbStep *step)
{
    fputs("Calon tiap garis (B = baris, K = kolom; Fb/Fk = jumlah minimum baris/kolom):\n", out);
    fputs("Garis  Sel          FLC  SLC  TLC  MAX  Pen  Bobot  Skor  Alokasi   Fb   Fk  Min  Jumlah\n", out);
    for (int i = 0; i < step->line_count; ++i) {
        const VrbLine *line = &step->lines[i];
        char cell[32];
        snprintf(cell, sizeof cell, "(%d,%d)", line->row + 1, line->column + 1);
        fprintf(out, "%c%-5d %-10s %4" PRId64 " %4" PRId64 " %4" PRId64 " %4" PRId64
                     " %4" PRId64 " %6" PRId64 " %5" PRId64 " %8" PRId64 " %4d %4d %4d %7d\n",
                line->axis == VRB_ROW ? 'B' : 'K', line->index + 1, cell,
                line->flc, line->slc, line->tlc, line->max, line->penalty, line->weight,
                line->score, line->quantity, step->row_counts[line->row],
                step->column_counts[line->column], minimum_count(step, line), total_count(step, line));
    }
}

static void print_reason(FILE *out, const VrbStep *step)
{
    const VrbLine *chosen = &step->lines[step->selected];
    if (step->alternative < 0) {
        fputs("Alasan: seluruh calon menunjuk sel yang sama.\n", out);
        return;
    }
    const VrbLine *other = &step->lines[step->alternative];
    switch (step->reason) {
    case VRB_SCORE:
        fprintf(out, "Alasan: skor %" PRId64 " lebih besar dari %" PRId64, chosen->score, other->score);
        break;
    case VRB_FLC:
        fprintf(out, "Alasan: skor sama, FLC %" PRId64 " lebih kecil dari %" PRId64, chosen->flc, other->flc);
        break;
    case VRB_MIN_COUNT:
        fprintf(out, "Alasan: skor dan FLC sama, hitungan minimum %d lebih kecil dari %d",
                minimum_count(step, chosen), minimum_count(step, other));
        break;
    case VRB_TOTAL_COUNT:
        fprintf(out, "Alasan: skor, FLC, dan hitungan minimum sama. Jumlah %d lebih kecil dari %d",
                total_count(step, chosen), total_count(step, other));
        break;
    case VRB_QUANTITY:
        fprintf(out, "Alasan: skor, FLC, dan kelangkaan sama. Alokasi %" PRId64 " lebih besar dari %" PRId64,
                chosen->quantity, other->quantity);
        break;
    default:
        fputs("Alasan: nilai seri, indeks sel terkecil dipakai agar hasil konsisten", out);
        break;
    }
    fprintf(out, " dibandingkan calon di sel (%d,%d).\n", other->row + 1, other->column + 1);
}

static void text_step(FILE *out, const VrbSetup *setup, const VrbStep *step)
{
    const VrbLine *chosen = &step->lines[step->selected];
    const int row = chosen->row, column = chosen->column;
    const VrbProblem *problem = setup->problem;
    const VrbValue unit_cost = row < problem->rows && column < problem->columns
                               ? problem->costs[row][column] : 0;
    fprintf(out, "\nIterasi %d\n", step->iteration);
    print_active_costs(out, setup, step);
    fputs("Pasokan sebelum alokasi: ", out);
    print_vector(out, step->supply_before, setup->rows, false);
    fputs("Permintaan sebelum alokasi: ", out);
    print_vector(out, step->demand_before, setup->columns, false);
    print_candidates(out, step);
    fprintf(out, "Dipilih: %s %d, sel (%d,%d).\n", chosen->axis == VRB_ROW ? "baris" : "kolom",
            chosen->index + 1, row + 1, column + 1);
    print_reason(out, step);
    fprintf(out, "Alokasi: min(%" PRId64 ", %" PRId64 ") = %" PRId64 ".\n",
            step->supply_before[row], step->demand_before[column], chosen->quantity);
    fputs("Sisa pasokan: ", out);
    print_vector(out, step->supply_after, setup->rows, false);
    fputs("Sisa permintaan: ", out);
    print_vector(out, step->demand_after, setup->columns, false);
    fprintf(out, "Biaya langkah: %" PRId64 " x %" PRId64 " = %" PRId64 ".\n"
                 "Biaya kumulatif: %" PRId64 ".\n",
            chosen->quantity, unit_cost, step->step_cost, step->cumulative_cost);
}

static void on_step(void *context, const VrbSetup *setup, const VrbStep *step)
{
    const VrbOutput *output = context;
    if (output->json) json_step(output->output, setup, step);
    else text_step(output->output, setup, step);
}

VrbObserver vrb_output_observer(VrbOutput *output)
{
    return (VrbObserver){output, output->json || output->steps ? prepared : NULL,
                        output->steps ? on_step : NULL};
}

void vrb_print_result(const VrbOutput *output, const VrbResult *result)
{
    FILE *out = output->output;
    if (output->json) {
        if (output->steps) fputc(']', out);
        fprintf(out, ",\"iterations\":%d,\"cost\":%" PRId64 ",\"allocation\":",
                result->iterations, result->cost);
        print_matrix(out, &result->allocation, result->rows, result->columns, true);
        fputs(",\"valid\":true}\n", out);
    } else {
        if (output->steps) fputc('\n', out);
        fprintf(out, "Metode: VRB dengan prioritas kelangkaan FLC\nIterasi: %d\nTotal biaya: %" PRId64
                     "\nMatriks alokasi:\n", result->iterations, result->cost);
        print_matrix(out, &result->allocation, result->rows, result->columns, false);
    }
}
