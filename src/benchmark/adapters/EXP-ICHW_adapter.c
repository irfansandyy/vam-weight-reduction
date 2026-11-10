#include <strings.h>
#define main exp_ichw_interactive_main
#include "../methods/EXP-ICHW.c"
#undef main

static void benchmark_emit_double(const double *a, int m, int n) {
    printf("__RESULT__ %d %d\n", m, n);
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j)
            printf("%.17g%c", a[(size_t)i*n+j], j+1 == n ? '\n' : ' ');
    }
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s FILE\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "input error\n"); return 3; }
    Problem p = {0};
    if (fscanf(f, "%d%d", &p.rows, &p.cols) != 2 || p.rows <= 0 || p.cols <= 0) {
        fclose(f); return 3;
    }
    p.tcm = calloc((size_t)p.rows*p.cols, sizeof(*p.tcm));
    p.supply = calloc((size_t)p.rows, sizeof(*p.supply));
    p.demand = calloc((size_t)p.cols, sizeof(*p.demand));
    if (!p.tcm || !p.supply || !p.demand) { fclose(f); problem_free(&p); return 4; }
    int ok = 1;
    for (size_t k = 0; k < (size_t)p.rows*p.cols; ++k) ok &= fscanf(f, "%lf", p.tcm+k) == 1;
    for (int i = 0; i < p.rows; ++i) ok &= fscanf(f, "%lf", p.supply+i) == 1;
    for (int j = 0; j < p.cols; ++j) ok &= fscanf(f, "%lf", p.demand+j) == 1;
    fclose(f);
    if (!ok) { problem_free(&p); return 3; }
    AlgoResult a;
    if (!simplified_algorithm(p.tcm, p.rows, p.cols, p.supply, p.demand, 0, &a)) {
        problem_free(&p); return 4;
    }
    benchmark_emit_double(a.alloc, p.rows, p.cols);
    algo_free(&a);
    problem_free(&p);
    return 0;
}
