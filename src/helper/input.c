#include "input.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

static const char *read_value(FILE *input, VrbValue *value)
{
    char token[128], *end;
    if (fscanf(input, "%127s", token) != 1) return "jumlah nilai input kurang";
    if (strlen(token) == sizeof token - 1) {
        const int next = fgetc(input);
        if (next != EOF && !isspace(next)) return "angka input terlalu panjang";
    }
    errno = 0;
    const intmax_t parsed = strtoimax(token, &end, 10);
    if (errno == ERANGE || end == token || *end || parsed < 0 || parsed > INT64_MAX)
        return "input harus berupa bilangan bulat tak negatif dalam batas 64 bit";
    *value = (VrbValue)parsed;
    return NULL;
}

static const char *read_vector(FILE *input, VrbValue *values, int count)
{
    for (int i = 0; i < count; ++i) {
        const char *error = read_value(input, &values[i]);
        if (error) return error;
    }
    return NULL;
}

const char *vrb_read_problem(FILE *input, FILE *prompts, VrbProblem *problem)
{
    VrbValue dimensions[2];
    if (prompts) {
        fputs("Jumlah baris dan kolom: ", prompts);
        fflush(prompts);
    }
    const char *error = read_vector(input, dimensions, 2);
    if (error) return error;
    if (dimensions[0] < 1 || dimensions[0] > VRB_MAX_INPUT_DIMENSION ||
        dimensions[1] < 1 || dimensions[1] > VRB_MAX_INPUT_DIMENSION)
        return "jumlah baris dan kolom harus antara 1 dan 127";
    problem->rows = (int)dimensions[0];
    problem->columns = (int)dimensions[1];
    for (int i = 0; i < problem->rows; ++i) {
        if (prompts) {
            fprintf(prompts, "Biaya baris %d (%d nilai): ", i + 1, problem->columns);
            fflush(prompts);
        }
        error = read_vector(input, problem->costs[i], problem->columns);
        if (error) return error;
    }
    if (prompts) {
        fprintf(prompts, "Pasokan (%d nilai): ", problem->rows);
        fflush(prompts);
    }
    error = read_vector(input, problem->supply, problem->rows);
    if (error) return error;
    if (prompts) {
        fprintf(prompts, "Permintaan (%d nilai): ", problem->columns);
        fflush(prompts);
    }
    /* Some datasets include reference values after the margins. */
    return read_vector(input, problem->demand, problem->columns);
}
