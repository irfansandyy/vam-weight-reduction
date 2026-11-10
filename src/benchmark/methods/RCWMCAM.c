/*
RCWMCAM BASIC (TCM only, no tie-breaker beyond Python's natural order)
C99 conversion of the provided Python code.

MATCHES PYTHON LOGIC:
- two_min_penalty: difference of two smallest costs; if <2 => 0.0
- step 2: row_min_costs / col_min_costs over unmasked cols/rows
- step 3: base = min_cost * qty where qty uses first argmin index among unmasked (NumPy argmin behavior)
- step 4: penalty computed on available costs
- step 5: weight = base * penalty
- step 6: choose maximum weight; if tie, choose first row in ascending index, else first col
- step 7: allocate using min-cost cell in chosen row/col (first argmin), qty = min(supply, demand)
- masking: mask_rows[supply==0]=True, mask_cols[demand==0]=True (exact equality, like Python on float)
- loop condition mimics: while not allclose(supply,0) and not allclose(demand,0)
  using atol=1e-08 (rtol ignored since target is 0)

I/O:
- Mode 1: single file (verbose)
- Mode 2: folder (verbose=False)

Build:
  Linux/macOS: gcc -O3 -std=c99 -o RCWMCAM_BASIC RCWMCAM_BASIC.c -lm
  Windows (MinGW): gcc -O3 -std=c99 -o RCWMCAM_BASIC.exe RCWMCAM_BASIC.c -lm
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <dirent.h>
  #include <sys/stat.h>
#endif

/* -----------------------------
   Robust token reader (like Python)
   - ignore blank lines
   - accept spaces and commas
   - try int(token)
   - else filter digits and '-' then try int(filtered)
----------------------------- */

typedef struct {
    long long *a;
    size_t n;
    size_t cap;
} LLVec;

static void llv_init(LLVec *v) { v->a = NULL; v->n = 0; v->cap = 0; }
static void llv_free(LLVec *v) { free(v->a); v->a = NULL; v->n = 0; v->cap = 0; }

static int llv_push(LLVec *v, long long x) {
    if (v->n == v->cap) {
        size_t ncap = (v->cap == 0) ? 1024 : v->cap * 2;
        long long *na = (long long*)realloc(v->a, ncap * sizeof(long long));
        if (!na) return 0;
        v->a = na;
        v->cap = ncap;
    }
    v->a[v->n++] = x;
    return 1;
}

static void rstrip_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = '\0';
        n--;
    }
}

static int try_parse_int_full(const char *s, long long *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno != 0) return 0;
    if (end == s) return 0;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

static int parse_token_like_python(const char *tok, long long *out) {
    if (try_parse_int_full(tok, out)) return 1;

    char buf[512];
    size_t k = 0;
    for (size_t i = 0; tok[i] && k + 1 < sizeof(buf); i++) {
        unsigned char c = (unsigned char)tok[i];
        if (isdigit(c) || c == '-') buf[k++] = (char)c;
    }
    buf[k] = '\0';
    if (k == 0) return 0;

    if (try_parse_int_full(buf, out)) return 1;
    return 0;
}

static int read_tokens_from_file(const char *path, LLVec *out_tokens) {
    llv_init(out_tokens);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        rstrip_newline(line);

        /* emulate Python: line.strip(); if not line: continue */
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0') continue;

        char tok[1024];
        size_t tlen = 0;

        for (size_t i = 0;; i++) {
            char c = p[i];
            int is_delim = (c == '\0' || isspace((unsigned char)c) || c == ',');
            if (!is_delim) {
                if (tlen + 1 < sizeof(tok)) tok[tlen++] = c;
            } else {
                if (tlen > 0) {
                    tok[tlen] = '\0';
                    long long val;
                    if (parse_token_like_python(tok, &val)) {
                        if (!llv_push(out_tokens, val)) {
                            fclose(f);
                            llv_free(out_tokens);
                            return 0;
                        }
                    }
                    tlen = 0;
                }
                if (c == '\0') break;
            }
        }
    }

    fclose(f);
    return 1;
}

/* -----------------------------
   read_data_from_file (utils equivalent)
   Format:
   R C
   R*C costs
   R supply
   C demand
   optional: optimal/effective cost
----------------------------- */

typedef struct {
    int rows;
    int cols;
    long long *tcm;    /* rows*cols */
    long long *supply; /* rows */
    long long *demand; /* cols */
    long long optimal; /* optional */
    int has_optimal;
} Data;

static void data_free(Data *d) {
    free(d->tcm); free(d->supply); free(d->demand);
    d->tcm = NULL; d->supply = NULL; d->demand = NULL;
}

static inline long long TCM_AT(const long long *a, int cols, int r, int c) {
    return a[(size_t)r*(size_t)cols + (size_t)c];
}
static inline void SET_TCM_AT(long long *a, int cols, int r, int c, long long v) {
    a[(size_t)r*(size_t)cols + (size_t)c] = v;
}

static int read_data_from_file(const char *path, Data *out) {
    memset(out, 0, sizeof(*out));

    LLVec toks;
    if (!read_tokens_from_file(path, &toks)) return 0;
    if (toks.n < 2) { llv_free(&toks); return 0; }

    size_t idx = 0;
    int R = (int)toks.a[idx++]; 
    int C = (int)toks.a[idx++];

    if (R <= 0 || C <= 0) { llv_free(&toks); return 0; }

    size_t need = (size_t)R*(size_t)C + (size_t)R + (size_t)C;
    if (toks.n < idx + need) { llv_free(&toks); return 0; }

    out->rows = R;
    out->cols = C;
    out->tcm = (long long*)calloc((size_t)R*(size_t)C, sizeof(long long));
    out->supply = (long long*)calloc((size_t)R, sizeof(long long));
    out->demand = (long long*)calloc((size_t)C, sizeof(long long));
    if (!out->tcm || !out->supply || !out->demand) {
        data_free(out);
        llv_free(&toks);
        return 0;
    }

    for (int r = 0; r < R; r++) {
        for (int c = 0; c < C; c++) {
            SET_TCM_AT(out->tcm, C, r, c, toks.a[idx++]);
        }
    }

    for (int r = 0; r < R; r++) out->supply[r] = toks.a[idx++];
    for (int c = 0; c < C; c++) out->demand[c] = toks.a[idx++];

    out->has_optimal = 0;
    out->optimal = 0;
    if (idx < toks.n) {
        out->has_optimal = 1;
        out->optimal = toks.a[idx];
    }

    llv_free(&toks);
    return 1;
}

/* -----------------------------
   Helpers: allclose to zero (np.allclose(x,0) with default atol=1e-08)
----------------------------- */

static int allclose_zero(const double *a, int n) {
    const double atol = 1e-8;
    for (int i = 0; i < n; i++) {
        if (fabs(a[i]) > atol) return 0;
    }
    return 1;
}

/* -----------------------------
   two_min_penalty(costs): difference of two smallest
   (Python sorts costs then takes sorted[1]-sorted[0])
----------------------------- */

static double two_min_penalty_scan(const long long *vals, int n) {
    if (n < 2) return 0.0;

    long long min1 = LLONG_MAX;
    long long min2 = LLONG_MAX;

    for (int i = 0; i < n; i++) {
        long long x = vals[i];
        if (x < min1) { min2 = min1; min1 = x; }
        else if (x < min2) { min2 = x; }
    }
    if (min1 == LLONG_MAX || min2 == LLONG_MAX) return 0.0;
    return (double)(min2 - min1);
}

/* -----------------------------
   RCWMCAM BASIC solver
----------------------------- */

static long long solve_rcwmcam_basic(const long long *tcm, int rows, int cols,
                                    const long long *supply_in, const long long *demand_in,
                                    int verbose) {
    /* supply/demand are float in Python (numpy float) */
    double *supply = (double*)calloc((size_t)rows, sizeof(double));
    double *demand = (double*)calloc((size_t)cols, sizeof(double));
    if (!supply || !demand) { free(supply); free(demand); return -1; }

    for (int i = 0; i < rows; i++) supply[i] = (double)supply_in[i];
    for (int j = 0; j < cols; j++) demand[j] = (double)demand_in[j];

    double *alloc = (double*)calloc((size_t)rows*(size_t)cols, sizeof(double));
    unsigned char *mask_rows = (unsigned char*)calloc((size_t)rows, 1);
    unsigned char *mask_cols = (unsigned char*)calloc((size_t)cols, 1);

    if (!alloc || !mask_rows || !mask_cols) {
        free(supply); free(demand);
        free(alloc); free(mask_rows); free(mask_cols);
        return -1;
    }

    int iteration = 1;

    if (verbose) {
        printf("\n================ RCWMCAM BASIC ================\n");
        printf("Matriks TCM:\n");
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                printf("%lld%s", TCM_AT(tcm, cols, r, c), (c < cols-1) ? " " : "");
            }
            printf("\n");
        }
        printf("===============================================\n\n");
    }

    /* while not allclose(supply,0) and not allclose(demand,0) */
    while (!allclose_zero(supply, rows) && !allclose_zero(demand, cols)) {

        if (verbose) printf("--- Iterasi %d ---\n", iteration);

        /* STEP 2: row_min_costs, col_min_costs over unmasked */
        /* We'll compute arrays with flags like dict keys in ascending order */
        unsigned char *row_active = (unsigned char*)calloc((size_t)rows, 1);
        unsigned char *col_active = (unsigned char*)calloc((size_t)cols, 1);
        long long *row_min_cost = (long long*)calloc((size_t)rows, sizeof(long long));
        long long *col_min_cost = (long long*)calloc((size_t)cols, sizeof(long long));
        if (!row_active || !col_active || !row_min_cost || !col_min_cost) {
            free(row_active); free(col_active); free(row_min_cost); free(col_min_cost);
            break;
        }

        for (int i = 0; i < rows; i++) {
            if (mask_rows[i]) continue;
            /* min over ~mask_cols */
            long long mn = LLONG_MAX;
            int any = 0;
            for (int j = 0; j < cols; j++) {
                if (mask_cols[j]) continue;
                long long v = TCM_AT(tcm, cols, i, j);
                if (!any || v < mn) { mn = v; any = 1; }
            }
            if (any) {
                row_active[i] = 1;
                row_min_cost[i] = mn;
            }
        }

        for (int j = 0; j < cols; j++) {
            if (mask_cols[j]) continue;
            long long mn = LLONG_MAX;
            int any = 0;
            for (int i = 0; i < rows; i++) {
                if (mask_rows[i]) continue;
                long long v = TCM_AT(tcm, cols, i, j);
                if (!any || v < mn) { mn = v; any = 1; }
            }
            if (any) {
                col_active[j] = 1;
                col_min_cost[j] = mn;
            }
        }

        /* STEP 3: base = min_cost * qty where j/i is first argmin among unmasked */
        double *row_base = (double*)calloc((size_t)rows, sizeof(double));
        double *col_base = (double*)calloc((size_t)cols, sizeof(double));
        unsigned char *row_base_ok = (unsigned char*)calloc((size_t)rows, 1);
        unsigned char *col_base_ok = (unsigned char*)calloc((size_t)cols, 1);

        if (!row_base || !col_base || !row_base_ok || !col_base_ok) {
            free(row_active); free(col_active); free(row_min_cost); free(col_min_cost);
            free(row_base); free(col_base); free(row_base_ok); free(col_base_ok);
            break;
        }

        for (int i = 0; i < rows; i++) {
            if (!row_active[i]) continue;

            /* j = first argmin of tcm[i, ~mask_cols] */
            long long mn = LLONG_MAX;
            int bestj = -1;
            for (int j = 0; j < cols; j++) {
                if (mask_cols[j]) continue;
                long long v = TCM_AT(tcm, cols, i, j);
                if (bestj < 0 || v < mn) { mn = v; bestj = j; }
            }
            if (bestj >= 0) {
                double qty = fmin(supply[i], demand[bestj]);
                row_base[i] = (double)row_min_cost[i] * qty;
                row_base_ok[i] = 1;
            }
        }

        for (int j = 0; j < cols; j++) {
            if (!col_active[j]) continue;

            /* i = first argmin of tcm[~mask_rows, j] */
            long long mn = LLONG_MAX;
            int besti = -1;
            for (int i = 0; i < rows; i++) {
                if (mask_rows[i]) continue;
                long long v = TCM_AT(tcm, cols, i, j);
                if (besti < 0 || v < mn) { mn = v; besti = i; }
            }
            if (besti >= 0) {
                double qty = fmin(supply[besti], demand[j]);
                col_base[j] = (double)col_min_cost[j] * qty;
                col_base_ok[j] = 1;
            }
        }

        /* STEP 4: penalties (TwoMin) on available costs */
        double *row_penalty = (double*)calloc((size_t)rows, sizeof(double));
        double *col_penalty = (double*)calloc((size_t)cols, sizeof(double));
        unsigned char *row_pen_ok = (unsigned char*)calloc((size_t)rows, 1);
        unsigned char *col_pen_ok = (unsigned char*)calloc((size_t)cols, 1);

        if (!row_penalty || !col_penalty || !row_pen_ok || !col_pen_ok) {
            free(row_active); free(col_active); free(row_min_cost); free(col_min_cost);
            free(row_base); free(col_base); free(row_base_ok); free(col_base_ok);
            free(row_penalty); free(col_penalty); free(row_pen_ok); free(col_pen_ok);
            break;
        }

        /* temp buffer for costs slice */
        long long *tmp = (long long*)malloc((size_t)(rows > cols ? rows : cols) * sizeof(long long));

        for (int i = 0; i < rows; i++) {
            if (!row_active[i]) continue;
            int cnt = 0;
            for (int j = 0; j < cols; j++) {
                if (mask_cols[j]) continue;
                tmp[cnt++] = TCM_AT(tcm, cols, i, j);
            }
            row_penalty[i] = two_min_penalty_scan(tmp, cnt);
            row_pen_ok[i] = 1;
        }

        for (int j = 0; j < cols; j++) {
            if (!col_active[j]) continue;
            int cnt = 0;
            for (int i = 0; i < rows; i++) {
                if (mask_rows[i]) continue;
                tmp[cnt++] = TCM_AT(tcm, cols, i, j);
            }
            col_penalty[j] = two_min_penalty_scan(tmp, cnt);
            col_pen_ok[j] = 1;
        }

        free(tmp);

        /* STEP 5: weight = base * penalty */
        double *row_weight = (double*)calloc((size_t)rows, sizeof(double));
        double *col_weight = (double*)calloc((size_t)cols, sizeof(double));
        unsigned char *row_w_ok = (unsigned char*)calloc((size_t)rows, 1);
        unsigned char *col_w_ok = (unsigned char*)calloc((size_t)cols, 1);

        if (!row_weight || !col_weight || !row_w_ok || !col_w_ok) {
            free(row_active); free(col_active); free(row_min_cost); free(col_min_cost);
            free(row_base); free(col_base); free(row_base_ok); free(col_base_ok);
            free(row_penalty); free(col_penalty); free(row_pen_ok); free(col_pen_ok);
            free(row_weight); free(col_weight); free(row_w_ok); free(col_w_ok);
            break;
        }

        for (int i = 0; i < rows; i++) {
            if (row_base_ok[i] && row_pen_ok[i]) {
                row_weight[i] = row_base[i] * row_penalty[i];
                row_w_ok[i] = 1;
            }
        }
        for (int j = 0; j < cols; j++) {
            if (col_base_ok[j] && col_pen_ok[j]) {
                col_weight[j] = col_base[j] * col_penalty[j];
                col_w_ok[j] = 1;
            }
        }

        /* STEP 6: choose max_weight over all weights */
        int anyw = 0;
        double max_weight = 0.0;

        for (int i = 0; i < rows; i++) {
            if (!row_w_ok[i]) continue;
            if (!anyw || row_weight[i] > max_weight) { max_weight = row_weight[i]; anyw = 1; }
        }
        for (int j = 0; j < cols; j++) {
            if (!col_w_ok[j]) continue;
            if (!anyw || col_weight[j] > max_weight) { max_weight = col_weight[j]; anyw = 1; }
        }

        if (!anyw) {
            /* should not happen in balanced case */
            free(row_active); free(col_active); free(row_min_cost); free(col_min_cost);
            free(row_base); free(col_base); free(row_base_ok); free(col_base_ok);
            free(row_penalty); free(col_penalty); free(row_pen_ok); free(col_pen_ok);
            free(row_weight); free(col_weight); free(row_w_ok); free(col_w_ok);
            break;
        }

        /* chosen: first row with w == max_weight, else first col with w == max_weight */
        int chosen_type_row = -1; /* 1=row, 0=col */
        int chosen_idx = -1;

        for (int i = 0; i < rows; i++) {
            if (row_w_ok[i] && row_weight[i] == max_weight) {
                chosen_type_row = 1;
                chosen_idx = i;
                break;
            }
        }
        if (chosen_type_row < 0) {
            for (int j = 0; j < cols; j++) {
                if (col_w_ok[j] && col_weight[j] == max_weight) {
                    chosen_type_row = 0;
                    chosen_idx = j;
                    break;
                }
            }
        }

        /* STEP 7: allocate */
        if (chosen_type_row == 1) {
            int idx = chosen_idx;

            /* j = first argmin in row idx among ~mask_cols */
            long long mn = LLONG_MAX;
            int bestj = -1;
            for (int j = 0; j < cols; j++) {
                if (mask_cols[j]) continue;
                long long v = TCM_AT(tcm, cols, idx, j);
                if (bestj < 0 || v < mn) { mn = v; bestj = j; }
            }

            if (bestj >= 0) {
                double qty = fmin(supply[idx], demand[bestj]);
                alloc[(size_t)idx*(size_t)cols + (size_t)bestj] += qty;
                supply[idx] -= qty;
                demand[bestj] -= qty;

                if (verbose) {
                    printf("  Alokasi dari Baris S%d -> D%d = %lld\n",
                           idx+1, bestj+1, (long long)qty);
                }
            }
        } else {
            int idx = chosen_idx;

            /* i = first argmin in col idx among ~mask_rows */
            long long mn = LLONG_MAX;
            int besti = -1;
            for (int i = 0; i < rows; i++) {
                if (mask_rows[i]) continue;
                long long v = TCM_AT(tcm, cols, i, idx);
                if (besti < 0 || v < mn) { mn = v; besti = i; }
            }

            if (besti >= 0) {
                double qty = fmin(supply[besti], demand[idx]);
                alloc[(size_t)besti*(size_t)cols + (size_t)idx] += qty;
                supply[besti] -= qty;
                demand[idx] -= qty;

                if (verbose) {
                    printf("  Alokasi dari Baris S%d -> D%d = %lld\n",
                           besti+1, idx+1, (long long)qty);
                }
            }
        }

        /* mask_rows[supply==0]=True; mask_cols[demand==0]=True */
        for (int i = 0; i < rows; i++) {
            if (supply[i] == 0.0) mask_rows[i] = 1;
        }
        for (int j = 0; j < cols; j++) {
            if (demand[j] == 0.0) mask_cols[j] = 1;
        }

        iteration++;
        if (verbose) {
            printf("  Sisa supply : ");
            for (int i = 0; i < rows; i++) printf("%lld%s", (long long)supply[i], (i<rows-1)?" ":"\n");
            printf("  Sisa demand : ");
            for (int j = 0; j < cols; j++) printf("%lld%s", (long long)demand[j], (j<cols-1)?" ":"\n");
            printf("----------------------------------\n");
        }

        /* free step buffers */
        free(row_active); free(col_active); free(row_min_cost); free(col_min_cost);
        free(row_base); free(col_base); free(row_base_ok); free(col_base_ok);
        free(row_penalty); free(col_penalty); free(row_pen_ok); free(col_pen_ok);
        free(row_weight); free(col_weight); free(row_w_ok); free(col_w_ok);
    }

    /* total_cost = float(np.sum(allocations * tcm)) then int(total_cost) */
    double total_cost_f = 0.0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            total_cost_f += alloc[(size_t)r*(size_t)cols + (size_t)c] * (double)TCM_AT(tcm, cols, r, c);
        }
    }
    long long total_cost = (long long)(total_cost_f); /* trunc toward zero (Python int(float)) */

    if (verbose) {
        printf("\n=== HASIL AKHIR RCWMCAM BASIC ===\n");
        printf("Matriks Alokasi:\n");
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                printf("%lld%s", (long long)alloc[(size_t)r*(size_t)cols + (size_t)c], (c<cols-1)?" ":"");
            }
            printf("\n");
        }
        printf("Total Biaya = %lld\n", total_cost);
        printf("==============================\n\n");
    }

    free(supply); free(demand);
    free(alloc); free(mask_rows); free(mask_cols);

    return total_cost;
}

/* -----------------------------
   Folder listing (sorted like Python sorted())
----------------------------- */

typedef struct {
    char **items;
    size_t n;
    size_t cap;
} StrList;

static char* str_dup(const char *s) {
    size_t n = strlen(s);
    char *p = (char*)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void sl_init(StrList *sl) { sl->items = NULL; sl->n = 0; sl->cap = 0; }
static void sl_free(StrList *sl) {
    if (!sl) return;
    for (size_t i = 0; i < sl->n; i++) free(sl->items[i]);
    free(sl->items);
    sl->items = NULL; sl->n = 0; sl->cap = 0;
}
static int sl_push(StrList *sl, const char *s) {
    if (sl->n == sl->cap) {
        size_t ncap = sl->cap ? sl->cap * 2 : 32;
        char **ni = (char**)realloc(sl->items, ncap * sizeof(char*));
        if (!ni) return 0;
        sl->items = ni;
        sl->cap = ncap;
    }
    char *cpy = str_dup(s);
    if (!cpy) return 0;
    sl->items[sl->n++] = cpy;
    return 1;
}
static int cmp_cstr(const void *a, const void *b) {
    const char *sa = *(const char* const*)a;
    const char *sb = *(const char* const*)b;
    return strcmp(sa, sb);
}

static int list_txt_files(const char *folder, StrList *out) {
    sl_init(out);

#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.txt", folder);

    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern, &ffd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        size_t n = strlen(ffd.cFileName);
        if (n >= 4) {
            const char *ext = ffd.cFileName + (n - 4);
            if (_stricmp(ext, ".txt") == 0) {
                if (!sl_push(out, ffd.cFileName)) { FindClose(h); sl_free(out); return 0; }
            }
        }
    } while (FindNextFileA(h, &ffd));
    FindClose(h);
#else
    DIR *dir = opendir(folder);
    if (!dir) return 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_DIR) continue;
        size_t n = strlen(ent->d_name);
        if (n >= 4) {
            const char *ext = ent->d_name + (n - 4);
            if (strcasecmp(ext, ".txt") == 0) {
                if (!sl_push(out, ent->d_name)) { closedir(dir); sl_free(out); return 0; }
            }
        }
    }
    closedir(dir);
#endif

    qsort(out->items, out->n, sizeof(char*), cmp_cstr);
    return 1;
}

static int is_dir_exists(const char *path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return 0;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#endif
}

static int is_file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void join_path(char *out, size_t out_sz, const char *folder, const char *file) {
#ifdef _WIN32
    snprintf(out, out_sz, "%s\\%s", folder, file);
#else
    snprintf(out, out_sz, "%s/%s", folder, file);
#endif
}

/* -----------------------------
   Main (mode 1/2) like Python
----------------------------- */

int main(void) {
    printf("=== Program RCWMCAM BASIC ===\n");
    printf("1. Selesaikan 1 soal\n");
    printf("2. Selesaikan banyak soal dalam folder\n");
    printf("Pilih mode (1/2): ");

    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    rstrip_newline(buf);

    if (strcmp(buf, "1") == 0) {
        printf("Masukkan nama file .txt: ");
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        rstrip_newline(buf);

        if (!is_file_exists(buf)) {
            printf("File tidak ditemukan atau format salah.\n");
            return 0;
        }

        Data d;
        if (!read_data_from_file(buf, &d)) {
            printf("File tidak ditemukan atau format salah.\n");
            return 0;
        }

        long long hasil = solve_rcwmcam_basic(d.tcm, d.rows, d.cols, d.supply, d.demand, 1);
        if (hasil < 0) {
            printf("File tidak ditemukan atau format salah.\n");
            data_free(&d);
            return 0;
        }

        if (d.has_optimal) printf("TOTAL COST = %lld, Optimal = %lld\n", hasil, d.optimal);
        else              printf("TOTAL COST = %lld, Optimal = %lld\n", hasil, (long long)-1);

        data_free(&d);

    } else if (strcmp(buf, "2") == 0) {
        printf("Masukkan nama folder: ");
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        rstrip_newline(buf);

        if (!is_dir_exists(buf)) {
            printf("Folder tidak ditemukan atau format salah.\n");
            return 0;
        }

        StrList files;
        if (!list_txt_files(buf, &files)) {
            printf("Folder tidak ditemukan atau format salah.\n");
            return 0;
        }

        for (size_t k = 0; k < files.n; k++) {
            char fullpath[8192];
            join_path(fullpath, sizeof(fullpath), buf, files.items[k]);

            Data d;
            if (!read_data_from_file(fullpath, &d)) {
                /* Python except: print folder not found OR format wrong; but inside loop they'd crash.
                   We'll mimic "try" around whole block by skipping bad file silently. */
                continue;
            }

            long long hasil = solve_rcwmcam_basic(d.tcm, d.rows, d.cols, d.supply, d.demand, 0);
            if (hasil >= 0) printf("%s : %lld\n", files.items[k], hasil);

            data_free(&d);
        }

        sl_free(&files);

    } else {
        printf("Pilihan tidak valid.\n");
    }

    return 0;
}
