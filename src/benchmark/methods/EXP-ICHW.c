/*
simplified_rcwmcam_cli.c  (C99)

Konversi dari kode Python yang diberikan:
- Mode 1: interaktif file tunggal (step-by-step)
- Mode 2: batch folder (ringkas)
- Parser: membaca file line-by-line, mengambil baris yang tidak kosong (strip),
  kemudian:
  line[0] = "rows cols"
  next rows lines = matriks TCM
  next line = supply
  next line = demand
  next line = effective_result (int)
- Exponential matrix per baris:
  positive_costs = costs > 0
  avg_cost = mean(positive_costs)
  lambda = 1/avg_cost
  new[i,j] = 1 - exp(-lambda * x) jika tcm != 0 else 0
- Penalti: inverse_contra_harmonic_mean(vals):
  if size>0:
    sum_sq = sum(vals^2)
    sum_costs = sum(vals)
    if sum_costs == 0 -> 0
    return (sum_sq/sum_costs)^(-1) = sum_costs/sum_sq
  else -> 0
- Pilih penalti terendah (min). Tanpa tie-breaker eksplisit:
  Python: min(all_penalties.items(), key=lambda kv: kv[1]) -> jika seri, pilih yg pertama muncul.
  Karena dict dibangun row dulu (i naik) lalu col (j naik), di C kita scan row dulu lalu col
  dan jika equal, TIDAK mengganti (keep first).
- Pemilihan sel: nilai exponential terkecil pada kandidat cols/rows yang belum mask.
  Argmin NumPy -> ambil indeks pertama jika seri.
- Loop utama: while (not allclose(supply,0)) OR (not allclose(demand,0))  (OR!)
  allclose ke 0: abs(x) <= 1e-08 (rtol tidak berpengaruh karena target 0)
- Mask pakai isclose ke 0: abs(x) <= 1e-08
- Total biaya: int(sum(alloc * tcm)) (truncate)

Build:
  gcc -O3 -std=c99 -o simplified_rcwmcam_cli simplified_rcwmcam_cli.c -lm
  (Windows MinGW) gcc -O3 -std=c99 -o simplified_rcwmcam_cli.exe simplified_rcwmcam_cli.c -lm
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <dirent.h>
  #include <sys/stat.h>
#endif

/* =========================
   Small helpers
   ========================= */

static void rstrip_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = '\0';
        n--;
    }
}

static char* lstrip(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rstrip_spaces(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) {
        s[n-1] = '\0';
        n--;
    }
}

static int is_file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
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

static int ends_with_txt_case_insensitive(const char *name) {
    size_t n = strlen(name);
    if (n < 4) return 0;
    const char *ext = name + (n - 4);
    return (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 't' &&
            tolower((unsigned char)ext[2]) == 'x' &&
            tolower((unsigned char)ext[3]) == 't');
}

static void join_path(char *out, size_t out_sz, const char *folder, const char *file) {
#ifdef _WIN32
    snprintf(out, out_sz, "%s\\%s", folder, file);
#else
    snprintf(out, out_sz, "%s/%s", folder, file);
#endif
}

static int cmp_cstr(const void *a, const void *b) {
    const char *sa = *(const char* const*)a;
    const char *sb = *(const char* const*)b;
    return strcmp(sa, sb);
}

static char* str_dup(const char *s) {
    size_t n = strlen(s);
    char *p = (char*)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* =========================
   Read non-empty lines into array (like Python: [line.strip() for line in f if line.strip()])
   ========================= */

typedef struct {
    char **a;
    size_t n;
    size_t cap;
} StrVec;

static void sv_init(StrVec *v) { v->a = NULL; v->n = 0; v->cap = 0; }
static void sv_free(StrVec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->n; i++) free(v->a[i]);
    free(v->a);
    v->a = NULL; v->n = 0; v->cap = 0;
}

static int sv_push(StrVec *v, const char *s) {
    if (v->n == v->cap) {
        size_t ncap = (v->cap == 0) ? 256 : v->cap * 2;
        char **na = (char**)realloc(v->a, ncap * sizeof(char*));
        if (!na) return 0;
        v->a = na;
        v->cap = ncap;
    }
    char *cpy = str_dup(s);
    if (!cpy) return 0;
    v->a[v->n++] = cpy;
    return 1;
}

static int read_nonempty_lines(const char *filename, StrVec *out_lines) {
    sv_init(out_lines);

    FILE *f = fopen(filename, "r");
    if (!f) return 0;

    char buf[16384];
    while (fgets(buf, sizeof(buf), f)) {
        rstrip_newline(buf);

        char *p = lstrip(buf);
        if (*p == '\0') continue;

        rstrip_spaces(p);
        if (*p == '\0') continue;

        if (!sv_push(out_lines, p)) {
            fclose(f);
            sv_free(out_lines);
            return 0;
        }
    }

    fclose(f);
    return 1;
}

/* =========================
   Parse a line into doubles (space-separated ints in Python)
   ========================= */

static int parse_doubles_from_line(const char *line, double *out, int needed) {
    int count = 0;
    const char *p = line;

    while (*p && count < needed) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char *end = NULL;
        double v = strtod(p, &end);
        if (end == p) {
            return 0;
        }
        out[count++] = v;
        p = end;
    }
    return (count == needed);
}

static int parse_int_from_line(const char *line, int *out) {
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p) return 0;
    *out = (int)v;
    return 1;
}

static int parse_two_ints_from_line(const char *line, int *a, int *b) {
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;

    char *end = NULL;
    long v1 = strtol(p, &end, 10);
    if (end == p) return 0;
    p = end;

    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;

    long v2 = strtol(p, &end, 10);
    if (end == p) return 0;

    *a = (int)v1;
    *b = (int)v2;
    return 1;
}

/* =========================
   read_data_from_file (same behavior as Python version)
   ========================= */

typedef struct {
    int rows;
    int cols;
    double *tcm;     /* rows*cols */
    double *supply;  /* rows */
    double *demand;  /* cols */
    int effective_result;
} Problem;

static void problem_free(Problem *p) {
    free(p->tcm);
    free(p->supply);
    free(p->demand);
    p->tcm = NULL; p->supply = NULL; p->demand = NULL;
}

static inline double MAT_AT(const double *a, int cols, int r, int c) {
    return a[(size_t)r*(size_t)cols + (size_t)c];
}
static inline void MAT_SET(double *a, int cols, int r, int c, double v) {
    a[(size_t)r*(size_t)cols + (size_t)c] = v;
}

static int read_data_from_file(const char *filename, Problem *out, char *errbuf, size_t errsz) {
    memset(out, 0, sizeof(*out));

    if (!is_file_exists(filename)) {
        snprintf(errbuf, errsz, "File '%s' tidak ditemukan.", filename);
        return 0;
    }

    StrVec lines;
    if (!read_nonempty_lines(filename, &lines)) {
        snprintf(errbuf, errsz, "Format file tidak sesuai atau terlalu pendek.");
        return 0;
    }

    if (lines.n < 3) {
        sv_free(&lines);
        snprintf(errbuf, errsz, "Format file tidak sesuai atau terlalu pendek.");
        return 0;
    }

    int rows = 0, cols = 0;
    if (!parse_two_ints_from_line(lines.a[0], &rows, &cols)) {
        sv_free(&lines);
        snprintf(errbuf, errsz, "Baris pertama harus berisi dua angka: <rows> <cols>.");
        return 0;
    }

    /* expected_lines = 1 + rows + 3 */
    size_t expected_lines = 1 + (size_t)rows + 3;
    if (lines.n < expected_lines) {
        sv_free(&lines);
        snprintf(errbuf, errsz, "File kurang baris. Diharapkan minimal %zu baris.", expected_lines);
        return 0;
    }

    out->rows = rows;
    out->cols = cols;
    out->tcm = (double*)calloc((size_t)rows*(size_t)cols, sizeof(double));
    out->supply = (double*)calloc((size_t)rows, sizeof(double));
    out->demand = (double*)calloc((size_t)cols, sizeof(double));
    if (!out->tcm || !out->supply || !out->demand) {
        sv_free(&lines);
        problem_free(out);
        snprintf(errbuf, errsz, "Memory allocation failed.");
        return 0;
    }

    /* tcm lines: lines[1 : 1+rows] */
    double *tmp = (double*)malloc((size_t)cols * sizeof(double));
    if (!tmp) {
        sv_free(&lines);
        problem_free(out);
        snprintf(errbuf, errsz, "Memory allocation failed.");
        return 0;
    }

    for (int i = 0; i < rows; i++) {
        const char *ln = lines.a[1 + (size_t)i];
        if (!parse_doubles_from_line(ln, tmp, cols)) {
            free(tmp);
            sv_free(&lines);
            problem_free(out);
            snprintf(errbuf, errsz, "Format matriks TCM tidak valid.");
            return 0;
        }
        for (int j = 0; j < cols; j++) {
            MAT_SET(out->tcm, cols, i, j, tmp[j]);
        }
    }

    /* supply line: lines[1+rows] (rows numbers) */
    if (!parse_doubles_from_line(lines.a[1 + (size_t)rows], out->supply, rows)) {
        free(tmp);
        sv_free(&lines);
        problem_free(out);
        snprintf(errbuf, errsz, "Format supply tidak valid.");
        return 0;
    }

    /* demand line: lines[1+rows+1] (cols numbers) */
    if (!parse_doubles_from_line(lines.a[1 + (size_t)rows + 1], out->demand, cols)) {
        free(tmp);
        sv_free(&lines);
        problem_free(out);
        snprintf(errbuf, errsz, "Format demand tidak valid.");
        return 0;
    }

    /* effective_result line: lines[1+rows+2] (int) */
    int eff = 0;
    if (!parse_int_from_line(lines.a[1 + (size_t)rows + 2], &eff)) {
        free(tmp);
        sv_free(&lines);
        problem_free(out);
        snprintf(errbuf, errsz, "Format effective_result tidak valid.");
        return 0;
    }
    out->effective_result = eff;

    free(tmp);
    sv_free(&lines);
    return 1;
}

/* =========================
   get_exponential_matrix (same)
   ========================= */

static double* get_exponential_matrix(const double *tcm, int rows, int cols) {
    double *exp_m = (double*)calloc((size_t)rows*(size_t)cols, sizeof(double));
    if (!exp_m) return NULL;

    for (int i = 0; i < rows; i++) {
        /* positive_costs = row_costs[row_costs > 0] */
        double sum_pos = 0.0;
        int cnt_pos = 0;
        for (int j = 0; j < cols; j++) {
            double x = MAT_AT(tcm, cols, i, j);
            if (x > 0.0) {
                sum_pos += x;
                cnt_pos++;
            }
        }

        if (cnt_pos > 0) {
            double avg_cost = sum_pos / (double)cnt_pos;
            double lambda_val = 1.0 / avg_cost;

            for (int j = 0; j < cols; j++) {
                double x = MAT_AT(tcm, cols, i, j);
                if (x != 0.0) {
                    exp_m[(size_t)i*(size_t)cols + (size_t)j] = 1.0 - exp(-lambda_val * x);
                } else {
                    exp_m[(size_t)i*(size_t)cols + (size_t)j] = 0.0;
                }
            }
        } else {
            for (int j = 0; j < cols; j++) {
                exp_m[(size_t)i*(size_t)cols + (size_t)j] = 0.0;
            }
        }
    }

    return exp_m;
}

/* =========================
   inverse_contra_harmonic_mean (same)
   ========================= */

static double inverse_contra_harmonic_mean(const double *vals, int n) {
    if (n > 0) {
        double sum_sq = 0.0;
        double sum_costs = 0.0;
        for (int i = 0; i < n; i++) {
            double x = vals[i];
            sum_sq += x * x;
            sum_costs += x;
        }
        if (sum_costs == 0.0) return 0.0;

        /* (sum_sq/sum_costs)^(-1) = sum_costs/sum_sq
           sum_sq==0 tidak mungkin jika sum_costs!=0 dalam kasus ini. */
        return (sum_costs / sum_sq);
    }
    return 0.0;
}

/* =========================
   allclose / isclose to 0 (np.allclose(x,0) / np.isclose(x,0) with default atol=1e-08)
   ========================= */

static int allclose_zero_arr(const double *a, int n) {
    const double atol = 1e-8;
    for (int i = 0; i < n; i++) {
        if (fabs(a[i]) > atol) return 0;
    }
    return 1;
}

static int isclose_zero(double x) {
    const double atol = 1e-8;
    return (fabs(x) <= atol);
}

/* =========================
   Printing helpers (approx numpy-style)
   ========================= */

static void print_int_matrix_like(const double *a, int rows, int cols) {
    printf("[");
    for (int i = 0; i < rows; i++) {
        printf("[");
        for (int j = 0; j < cols; j++) {
            long long v = (long long)MAT_AT(a, cols, i, j);
            printf("%lld%s", v, (j < cols-1) ? " " : "");
        }
        printf("]%s", (i < rows-1) ? "\n " : "");
    }
    printf("]\n");
}

static void print_double_matrix_round6_like(const double *a, int rows, int cols) {
    printf("[");
    for (int i = 0; i < rows; i++) {
        printf("[");
        for (int j = 0; j < cols; j++) {
            double v = a[(size_t)i*(size_t)cols + (size_t)j];
            /* emulate np.round(x,6) */
            double r = nearbyint(v * 1e6) / 1e6;
            printf("%.6f%s", r, (j < cols-1) ? " " : "");
        }
        printf("]%s", (i < rows-1) ? "\n " : "");
    }
    printf("]\n");
}

static void print_int_vector_like(const double *a, int n) {
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%lld%s", (long long)a[i], (i < n-1) ? " " : "");
    }
    printf("]\n");
}

/* =========================
   simplified_algorithm (same)
   Returns: allocations, total_cost, exp_matrix
   In C: we output allocations + exp_matrix via pointers
   ========================= */

typedef struct {
    double *alloc;      /* rows*cols */
    long long total_cost;
    double *exp_matrix; /* rows*cols */
} AlgoResult;

static void algo_free(AlgoResult *r) {
    free(r->alloc);
    free(r->exp_matrix);
    r->alloc = NULL;
    r->exp_matrix = NULL;
}

static int simplified_algorithm(const double *tcm, int rows, int cols,
                                const double *supply_in, const double *demand_in,
                                int verbose,
                                AlgoResult *out) {
    memset(out, 0, sizeof(*out));

    double *exp_matrix = get_exponential_matrix(tcm, rows, cols);
    if (!exp_matrix) return 0;

    double *alloc = (double*)calloc((size_t)rows*(size_t)cols, sizeof(double));
    double *current_supply = (double*)calloc((size_t)rows, sizeof(double));
    double *current_demand = (double*)calloc((size_t)cols, sizeof(double));
    unsigned char *mask_rows = (unsigned char*)calloc((size_t)rows, 1);
    unsigned char *mask_cols = (unsigned char*)calloc((size_t)cols, 1);

    if (!alloc || !current_supply || !current_demand || !mask_rows || !mask_cols) {
        free(exp_matrix);
        free(alloc); free(current_supply); free(current_demand);
        free(mask_rows); free(mask_cols);
        return 0;
    }

    for (int i = 0; i < rows; i++) current_supply[i] = supply_in[i];
    for (int j = 0; j < cols; j++) current_demand[j] = demand_in[j];

    if (verbose) {
        printf("--- Algoritma (Exponential + Harmonic, TANPA tie-breaker) ---\n");
        printf("Matriks TCM (asli):\n");
        print_int_matrix_like(tcm, rows, cols);
        printf("Matriks Exponential (digunakan untuk penalti):\n");
        print_double_matrix_round6_like(exp_matrix, rows, cols);
        printf("Supply awal: "); print_int_vector_like(current_supply, rows);
        printf("Demand awal: "); print_int_vector_like(current_demand, cols);
        printf("==================================================\n");
    }

    int iteration = 1;

    while ((!allclose_zero_arr(current_supply, rows)) || (!allclose_zero_arr(current_demand, cols))) {
        if (iteration > 10000) {
            if (verbose) printf("Batas iterasi tercapai, menghentikan loop.\n");
            break;
        }

        /* compute penalties for each unmasked row/col */
        /* Python stores in dict: row_0.. then col_0.. ; min() picks first if tie.
           We'll find global min by scanning rows then cols, and keep first on equal. */

        int best_is_row = 1;
        int best_idx = -1;
        double best_pen = 0.0;
        int has_best = 0;

        /* verbose needs to print penalty dicts (rounded) */
        double *row_pen = (double*)calloc((size_t)rows, sizeof(double));
        double *col_pen = (double*)calloc((size_t)cols, sizeof(double));
        unsigned char *row_pen_ok = (unsigned char*)calloc((size_t)rows, 1);
        unsigned char *col_pen_ok = (unsigned char*)calloc((size_t)cols, 1);

        if (!row_pen || !col_pen || !row_pen_ok || !col_pen_ok) {
            free(row_pen); free(col_pen); free(row_pen_ok); free(col_pen_ok);
            break;
        }

        /* temp buffer for vals slice */
        int maxdim = (rows > cols) ? rows : cols;
        double *tmp = (double*)malloc((size_t)maxdim * sizeof(double));
        if (!tmp) {
            free(row_pen); free(col_pen); free(row_pen_ok); free(col_pen_ok);
            break;
        }

        for (int i = 0; i < rows; i++) {
            if (mask_rows[i]) continue;
            int cnt = 0;
            for (int j = 0; j < cols; j++) {
                if (mask_cols[j]) continue;
                tmp[cnt++] = exp_matrix[(size_t)i*(size_t)cols + (size_t)j];
            }
            double pen = inverse_contra_harmonic_mean(tmp, cnt);
            row_pen[i] = pen;
            row_pen_ok[i] = 1;

            if (!has_best || pen < best_pen) {
                has_best = 1;
                best_pen = pen;
                best_is_row = 1;
                best_idx = i;
            }
            /* if equal, keep earlier (do nothing) */
        }

        for (int j = 0; j < cols; j++) {
            if (mask_cols[j]) continue;
            int cnt = 0;
            for (int i = 0; i < rows; i++) {
                if (mask_rows[i]) continue;
                tmp[cnt++] = exp_matrix[(size_t)i*(size_t)cols + (size_t)j];
            }
            double pen = inverse_contra_harmonic_mean(tmp, cnt);
            col_pen[j] = pen;
            col_pen_ok[j] = 1;

            if (!has_best || pen < best_pen) {
                has_best = 1;
                best_pen = pen;
                best_is_row = 0;
                best_idx = j;
            }
            /* if equal, keep earlier (Python min keeps first) */
        }

        free(tmp);

        if (!has_best) {
            free(row_pen); free(col_pen); free(row_pen_ok); free(col_pen_ok);
            break;
        }

        if (verbose) {
            printf("Iterasi %d:\n", iteration);

            printf("  Penalti baris: {");
            int first = 1;
            for (int i = 0; i < rows; i++) {
                if (!row_pen_ok[i]) continue;
                double rr = nearbyint(row_pen[i]*1e6)/1e6;
                if (!first) printf(", ");
                printf("'row_%d': %.6f", i, rr);
                first = 0;
            }
            printf("}\n");

            printf("  Penalti kolom: {");
            first = 1;
            for (int j = 0; j < cols; j++) {
                if (!col_pen_ok[j]) continue;
                double rr = nearbyint(col_pen[j]*1e6)/1e6;
                if (!first) printf(", ");
                printf("'col_%d': %.6f", j, rr);
                first = 0;
            }
            printf("}\n");

            if (best_is_row) {
                printf("  Prioritas terpilih: row_%d (penalti=%.6f)\n", best_idx, best_pen);
            } else {
                printf("  Prioritas terpilih: col_%d (penalti=%.6f)\n", best_idx, best_pen);
            }
        }

        /* Choose best cell on selected row/col: minimum exponential value among unmasked candidates (first if tie) */
        int i_sel = -1, j_sel = -1;

        if (best_is_row) {
            int i = best_idx;

            /* candidate_cols = where ~mask_cols */
            int any = 0;
            double bestv = 0.0;
            int bestj = -1;

            for (int j = 0; j < cols; j++) {
                if (mask_cols[j]) continue;
                double v = exp_matrix[(size_t)i*(size_t)cols + (size_t)j];
                if (!any || v < bestv) {
                    any = 1;
                    bestv = v;
                    bestj = j;
                }
                /* if equal, keep first (argmin behavior) */
            }

            if (!any) {
                mask_rows[i] = 1;
                if (verbose) printf("  Baris %d tidak punya kolom tersisa -> di-mask.\n", i+1);
                free(row_pen); free(col_pen); free(row_pen_ok); free(col_pen_ok);
                iteration++;
                continue;
            }

            i_sel = i;
            j_sel = bestj;
        } else {
            int j = best_idx;

            int any = 0;
            double bestv = 0.0;
            int besti = -1;

            for (int i = 0; i < rows; i++) {
                if (mask_rows[i]) continue;
                double v = exp_matrix[(size_t)i*(size_t)cols + (size_t)j];
                if (!any || v < bestv) {
                    any = 1;
                    bestv = v;
                    besti = i;
                }
                /* equal -> keep first */
            }

            if (!any) {
                mask_cols[j] = 1;
                if (verbose) printf("  Kolom %d tidak punya baris tersisa -> di-mask.\n", j+1);
                free(row_pen); free(col_pen); free(row_pen_ok); free(col_pen_ok);
                iteration++;
                continue;
            }

            i_sel = besti;
            j_sel = j;
        }

        /* Allocation qty = min(current_supply[i], current_demand[j]) */
        double qty = current_supply[i_sel] < current_demand[j_sel] ? current_supply[i_sel] : current_demand[j_sel];
        alloc[(size_t)i_sel*(size_t)cols + (size_t)j_sel] += qty;
        current_supply[i_sel] -= qty;
        current_demand[j_sel] -= qty;

        if (verbose) {
            printf("  Alokasi -> S%d ke D%d: %lld unit\n", i_sel+1, j_sel+1, (long long)qty);
            printf("  Sisa supply: "); print_int_vector_like(current_supply, rows);
            printf("  Sisa demand: "); print_int_vector_like(current_demand, cols);
        }

        if (isclose_zero(current_supply[i_sel])) {
            mask_rows[i_sel] = 1;
            if (verbose) printf("  S%d habis -> di-mask.\n", i_sel+1);
        }
        if (isclose_zero(current_demand[j_sel])) {
            mask_cols[j_sel] = 1;
            if (verbose) printf("  D%d habis -> di-mask.\n", j_sel+1);
        }

        if (verbose) printf("----------------------------------------\n");

        free(row_pen); free(col_pen); free(row_pen_ok); free(col_pen_ok);
        iteration++;
    }

    /* total_cost = int(sum(alloc * tcm)) */
    double total = 0.0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            total += alloc[(size_t)i*(size_t)cols + (size_t)j] * MAT_AT(tcm, cols, i, j);
        }
    }
    long long total_cost = (long long)total;

    if (verbose) {
        printf("==================================================\n");
        printf("Hasil akhir:\n");
        printf("Alokasi (jumlah unit per sel):\n");
        print_int_matrix_like(alloc, rows, cols);
        printf("Total biaya (menggunakan TCM asli): %lld\n", total_cost);
    }

    out->alloc = alloc;
    out->exp_matrix = exp_matrix;
    out->total_cost = total_cost;

    free(current_supply);
    free(current_demand);
    free(mask_rows);
    free(mask_cols);

    return 1;
}

/* =========================
   Mode 1 (interactive loop)
   ========================= */

static void run_single_file_interactive(void) {
    char filename[4096];

    while (1) {
        printf("Masukkan nama file data (mis. n1.txt) atau ketik 'exit' untuk selesai: ");
        if (!fgets(filename, sizeof(filename), stdin)) return;
        rstrip_newline(filename);

        /* strip spaces similar to .strip() */
        char *p = lstrip(filename);
        rstrip_spaces(p);

        if (strcasecmp(p, "exit") == 0 || strcasecmp(p, "quit") == 0) {
            printf("Keluar.\n");
            return;
        }
        if (*p == '\0') continue;

        if (!is_file_exists(p)) {
            printf("ERROR: File '%s' tidak ditemukan. Coba lagi.\n", p);
            continue;
        }

        Problem prob;
        char err[512];
        if (!read_data_from_file(p, &prob, err, sizeof(err))) {
            printf("Terjadi kesalahan saat membaca file: %s\n", err);
            continue;
        }

        /* check balance: int(sum(supply)) != int(sum(demand)) */
        double sumS = 0.0, sumD = 0.0;
        for (int i = 0; i < prob.rows; i++) sumS += prob.supply[i];
        for (int j = 0; j < prob.cols; j++) sumD += prob.demand[j];

        if ((long long)sumS != (long long)sumD) {
            printf("ERROR: Supply dan Demand tidak seimbang pada file tersebut.\n");
            problem_free(&prob);
            continue;
        }

        AlgoResult ar;
        if (!simplified_algorithm(prob.tcm, prob.rows, prob.cols, prob.supply, prob.demand, 1, &ar)) {
            printf("Terjadi kesalahan saat menjalankan algoritma.\n");
            problem_free(&prob);
            continue;
        }

        printf("Total biaya efektif (dari file): %d\n", prob.effective_result);
        if (ar.total_cost == (long long)prob.effective_result) {
            printf("-> Solusi yang ditemukan cocok dengan hasil efektif (sama).\n");
        } else {
            printf("-> Solusi feasible, mungkin bukan optimal (tidak cocok).\n");
        }

        printf("\nSelesai untuk file: %s\n", p);
        printf("------------------------------------------------------------\n");

        algo_free(&ar);
        problem_free(&prob);
    }
}

/* =========================
   Mode 2 (batch folder)
   ========================= */

typedef struct {
    char **items;
    size_t n;
    size_t cap;
} StrList;

static void sl_init(StrList *sl) { sl->items=NULL; sl->n=0; sl->cap=0; }
static void sl_free(StrList *sl) {
    if (!sl) return;
    for (size_t i=0;i<sl->n;i++) free(sl->items[i]);
    free(sl->items);
    sl->items=NULL; sl->n=0; sl->cap=0;
}
static int sl_push(StrList *sl, const char *s) {
    if (sl->n == sl->cap) {
        size_t ncap = sl->cap ? sl->cap*2 : 32;
        char **ni = (char**)realloc(sl->items, ncap*sizeof(char*));
        if (!ni) return 0;
        sl->items = ni;
        sl->cap = ncap;
    }
    char *cpy = str_dup(s);
    if (!cpy) return 0;
    sl->items[sl->n++] = cpy;
    return 1;
}

static int list_txt_files_sorted(const char *folder, StrList *out) {
    sl_init(out);

#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.*", folder);

    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern, &ffd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (ends_with_txt_case_insensitive(ffd.cFileName)) {
            if (!sl_push(out, ffd.cFileName)) { FindClose(h); sl_free(out); return 0; }
        }
    } while (FindNextFileA(h, &ffd));

    FindClose(h);
#else
    DIR *dir = opendir(folder);
    if (!dir) return 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_DIR) continue;
        if (ends_with_txt_case_insensitive(ent->d_name)) {
            if (!sl_push(out, ent->d_name)) { closedir(dir); sl_free(out); return 0; }
        }
    }
    closedir(dir);
#endif

    qsort(out->items, out->n, sizeof(char*), cmp_cstr);
    return 1;
}

static void run_batch_folder(const char *foldername) {
    if (!is_dir_exists(foldername)) {
        printf("ERROR: Folder '%s' tidak ditemukan atau bukan folder.\n", foldername);
        return;
    }

    StrList files;
    if (!list_txt_files_sorted(foldername, &files) || files.n == 0) {
        printf("Tidak ditemukan file .txt di folder '%s'.\n", foldername);
        sl_free(&files);
        return;
    }

    /* Python stores results then prints in order; we can print after compute by storing as strings */
    char **results = (char**)calloc(files.n, sizeof(char*));
    if (!results) {
        sl_free(&files);
        return;
    }

    for (size_t k = 0; k < files.n; k++) {
        char fullpath[8192];
        join_path(fullpath, sizeof(fullpath), foldername, files.items[k]);

        Problem prob;
        char err[512];
        if (!read_data_from_file(fullpath, &prob, err, sizeof(err))) {
            /* results[fname] = f"ERROR: {e}" */
            char buf[1024];
            snprintf(buf, sizeof(buf), "ERROR: %s", err);
            results[k] = str_dup(buf);
            continue;
        }

        double sumS = 0.0, sumD = 0.0;
        for (int i = 0; i < prob.rows; i++) sumS += prob.supply[i];
        for (int j = 0; j < prob.cols; j++) sumD += prob.demand[j];

        if ((long long)sumS != (long long)sumD) {
            results[k] = str_dup("ERROR: supply/demand tidak seimbang");
            problem_free(&prob);
            continue;
        }

        AlgoResult ar;
        if (!simplified_algorithm(prob.tcm, prob.rows, prob.cols, prob.supply, prob.demand, 0, &ar)) {
            results[k] = str_dup("ERROR: Terjadi kesalahan saat menjalankan algoritma.");
            problem_free(&prob);
            continue;
        }

        char buf[128];
        snprintf(buf, sizeof(buf), "%lld", ar.total_cost);
        results[k] = str_dup(buf);

        algo_free(&ar);
        problem_free(&prob);
    }

    for (size_t k = 0; k < files.n; k++) {
        printf("%s : %s\n", files.items[k], results[k] ? results[k] : "ERROR");
        free(results[k]);
    }

    free(results);
    sl_free(&files);
}

/* =========================
   Main
   ========================= */

int main(void) {
    printf("=== simplified_rcwmcam CLI ===\n");
    printf("Pilih mode:\n");
    printf("1. Satu file (interaktif, tampil step-by-step)\n");
    printf("2. Folder (batch, ringkas output untuk tiap .txt)\n");
    printf("Masukkan pilihan (1/2): ");

    char mode[64];
    if (!fgets(mode, sizeof(mode), stdin)) return 0;
    rstrip_newline(mode);
    char *m = lstrip(mode);
    rstrip_spaces(m);

    if (strcmp(m, "1") == 0) {
        run_single_file_interactive();
    } else if (strcmp(m, "2") == 0) {
        char folder[4096];
        printf("Masukkan path folder yang berisi file .txt: ");
        if (!fgets(folder, sizeof(folder), stdin)) return 0;
        rstrip_newline(folder);
        char *f = lstrip(folder);
        rstrip_spaces(f);
        run_batch_folder(f);
    } else {
        printf("Pilihan tidak valid. Keluar.\n");
    }

    return 0;
}
