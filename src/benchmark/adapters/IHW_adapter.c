#define main ihw_interactive_main
#include "../methods/IHW.c"
#undef main

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s FILE\n", argv[0]); return 2; }
    Problem p;
    if (!readTransportationProblemFile(argv[1], &p)) { fprintf(stderr, "input error\n"); return 3; }
    long long *a = calloc((size_t)p.m*p.n, sizeof(*a));
    if (!a) { freeProblem(&p); return 4; }
    (void)solveIHW(&p, 0, a);
    printf("__RESULT__ %d %d\n", p.m, p.n);
    for (int i = 0; i < p.m; ++i) {
        for (int j = 0; j < p.n; ++j)
            printf("%lld%c", a[(size_t)i*p.n+j], j+1 == p.n ? '\n' : ' ');
    }
    free(a);
    freeProblem(&p);
    return 0;
}
