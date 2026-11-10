/* Cost-Supply Method, numbered Steps 1-9 and Table II of:
 * R. F. Bunyamin, B. Amaliah and A. Saikhu (2026), ETASR 16(2),
 * 33024-33030, https://doi.org/10.48084/etasr.16468
 *
 * Independent C implementation. The paper's Figure 3 differs from Step 6
 * about equal-difference ties. This implementation follows the numbered
 * steps and the N32 worked example: quantity * destination unit cost, then
 * destination unit cost. Remaining ties use column/row index order.
 *
 * Usage: cc -std=c11 -O2 CSM.c -o csm; ./csm instance.txt [--trace]
 * Input: m n, cost matrix, supplies, demands. Optional trailing reference
 * optimum is ignored. Zero-cost dummy row/column balances unequal totals.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAXN 205
static int m, n, trace;
static long long c[MAXN][MAXN], x[MAXN][MAXN];
static long long s[MAXN], d[MAXN], tc[MAXN], cs[MAXN], ta[MAXN];

static long long minimum(long long a, long long b) { return a < b ? a : b; }

static int read_input(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 0; }
    if (fscanf(f, "%d%d", &m, &n) != 2 || m < 1 || n < 1 || m >= MAXN || n >= MAXN) {
        fprintf(stderr, "Invalid dimensions\n"); fclose(f); return 0;
    }
    long long ts = 0, td = 0;
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++)
        if (fscanf(f, "%lld", &c[i][j]) != 1 || c[i][j] < 0) { fclose(f); return 0; }
    for (int i = 0; i < m; i++) {
        if (fscanf(f, "%lld", &s[i]) != 1 || s[i] < 0) { fclose(f); return 0; }
        ts += s[i];
    }
    for (int j = 0; j < n; j++) {
        if (fscanf(f, "%lld", &d[j]) != 1 || d[j] < 0) { fclose(f); return 0; }
        td += d[j];
    }
    fclose(f);
    if (ts > td) d[n++] = ts - td;
    else if (td > ts) s[m++] = td - ts;
    return 1;
}

/* The higher-CS row is satisfied first, except equal original supplies
 * favor the FLC donor as prescribed in Step 7. A move may make the
 * receiver excessive, as in the original SSM rebalancing convention. */
static long long quantity(int donor, int receiver, int j) {
    int priority = (s[donor] == s[receiver] || cs[donor] >= cs[receiver]) ? donor : receiver;
    long long target = priority == donor ? ta[donor] - s[donor] : s[receiver] - ta[receiver];
    return minimum(x[donor][j], target);
}

static int solve(void) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) tc[i] += c[i][j];
        cs[i] = tc[i] * s[i];
    }
    for (int j = 0; j < n; j++) {
        int best = 0;
        for (int i = 1; i < m; i++)
            if (c[i][j] < c[best][j] || (c[i][j] == c[best][j] && tc[i] > tc[best])) best = i;
        x[best][j] = d[j]; ta[best] += d[j];
    }
    for (int iter = 1; iter <= 1000000; iter++) {
        int excessive = 0;
        for (int i = 0; i < m; i++) if (ta[i] > s[i]) excessive++;
        if (!excessive) return 1;
        int bd = -1, br = -1, bj = -1;
        long long bdiff = LLONG_MAX, bproduct = LLONG_MAX, bcost = LLONG_MAX, bq = 0;
        for (int j = 0; j < n; j++) {
            int donor = -1, receiver = -1;
            for (int i = 0; i < m; i++) {
                if (ta[i] > s[i] && x[i][j] > 0 &&
                    (donor < 0 || c[i][j] < c[donor][j] ||
                    (c[i][j] == c[donor][j] && tc[i] > tc[donor]))) donor = i;
                if (ta[i] < s[i] &&
                    (receiver < 0 || c[i][j] < c[receiver][j] ||
                    (c[i][j] == c[receiver][j] && tc[i] > tc[receiver]))) receiver = i;
            }
            if (donor < 0 || receiver < 0) continue;
            long long diff = llabs(c[receiver][j] - c[donor][j]);
            long long q = quantity(donor, receiver, j), product = q * c[receiver][j];
            if (diff < bdiff || (diff == bdiff && (product < bproduct ||
                (product == bproduct && c[receiver][j] < bcost)))) {
                bd = donor; br = receiver; bj = j;
                bdiff = diff; bproduct = product; bcost = c[receiver][j]; bq = q;
            }
        }
        if (bd < 0 || bq <= 0) { fprintf(stderr, "No admissible transfer\n"); return 0; }
        if (trace) printf("Transfer %d: row %d -> row %d, column %d, diff=%lld, quantity=%lld, destination product=%lld\n",
                          iter, bd+1, br+1, bj+1, bdiff, bq, bproduct);
        x[bd][bj] -= bq; x[br][bj] += bq; ta[bd] -= bq; ta[br] += bq;
    }
    fprintf(stderr, "Transfer safety limit reached\n"); return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s file [--trace]\n", argv[0]); return 1; }
    trace = argc > 2 && strcmp(argv[2], "--trace") == 0;
    if (!read_input(argv[1]) || !solve()) return 1;
    long long cost = 0;
    for (int i = 0; i < m; i++) {
        if (ta[i] != s[i]) { fprintf(stderr, "Infeasible row\n"); return 1; }
        for (int j = 0; j < n; j++) cost += c[i][j] * x[i][j];
    }
    printf("__RESULT__ %d %d\n", m, n);
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) printf("%lld ", x[i][j]);
        printf("\n");
    }
    printf("TCC = %lld\n", cost);
    return 0;
}
