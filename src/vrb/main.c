#include "input.h"
#include "trace.h"

#include <stdlib.h>
#include <string.h>

static int usage(FILE *out, const char *program, int status)
{
    fprintf(out, "Penggunaan: %s [FILE | --file FILE | --cli | --stdin] [--trace] [--json]\n"
                 "Tanpa sumber input, program meminta data melalui terminal.\n",
            program);
    return status;
}

static int fail(const char *message)
{
    fprintf(stderr, "VRB: %s\n", message);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    bool stdin_mode = false, prompts = true;
    VrbOutput output = {.output = stdout};
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) return usage(stdout, argv[0], EXIT_SUCCESS);
        if (strcmp(argv[i], "--trace") == 0) output.steps = true;
        else if (strcmp(argv[i], "--json") == 0) output.json = true;
        else if (strcmp(argv[i], "--cli") == 0 || strcmp(argv[i], "--stdin") == 0 ||
                 strcmp(argv[i], "-") == 0) {
            if (path || stdin_mode) return usage(stderr, argv[0], EXIT_FAILURE);
            stdin_mode = true;
            prompts = strcmp(argv[i], "--cli") == 0;
        } else {
            if (path || stdin_mode) return usage(stderr, argv[0], EXIT_FAILURE);
            if (strcmp(argv[i], "--file") == 0) {
                if (++i == argc) return usage(stderr, argv[0], EXIT_FAILURE);
            } else if (argv[i][0] == '-') return usage(stderr, argv[0], EXIT_FAILURE);
            path = argv[i];
        }
    }

    FILE *input = path ? fopen(path, "r") : stdin;
    if (!input) {
        perror(path);
        return EXIT_FAILURE;
    }
    static VrbProblem problem;
    static VrbResult result;
    const char *error = vrb_read_problem(input, !path && prompts ? stderr : NULL, &problem);
    if (path) fclose(input);
    if (error) return fail(error);

    const VrbObserver observer = vrb_output_observer(&output);
    switch (vrb_solve(&problem, &result, &observer)) {
    case VRB_OK:
        vrb_print_result(&output, &result);
        return ferror(stdout) ? EXIT_FAILURE : EXIT_SUCCESS;
    case VRB_INVALID_DIMENSIONS: return fail("jumlah baris dan kolom harus antara 1 dan 127");
    case VRB_NEGATIVE_INPUT: return fail("input tidak boleh negatif");
    case VRB_OVERFLOW: return fail("perhitungan melebihi batas bilangan bulat 64 bit");
    case VRB_INFEASIBLE: return fail("alokasi gagal memenuhi pasokan dan permintaan");
    }
    return fail("kesalahan tidak dikenal");
}
