/*
 * PowerRank.c — Power Rank Method
 * Kumari A., Bose T., Gupta G., Bala R., "Modified method for finding the
 * solution of balanced transportation problems: a power rank method,"
 * Computational Technologies, 2024, vol. 29, no. 2, pp. 62-68.
 *
 * Direct C translation of the paper's Steps 1-6:
 *   Step 2a/2b: reduce the cost matrix (subtract row min, then column min of
 *               the row-reduced matrix) so every active row and column has
 *               at least one zero; re-applied whenever a row/column loses
 *               its zero after an allocation removes rows/columns.
 *   Step 3:     find the cell with the LARGEST reduced cost among the
 *               active (not yet fully allocated) rows/columns.
 *   Step 4:     candidates = the zero cells lying in that cell's row or
 *               column; power rank(i,j) = average of the OTHER non-zero
 *               reduced costs in row i and column j.
 *   Step 5:     allocate at the candidate with the largest power rank
 *               (tie -> smallest quantity to allocate).
 *   Step 6:     repeat until every row/column is satisfied.
 *
 * Validated against the paper's own worked example (Numerical Problem 1,
 * Table 2): cost=[[13,18,30,8],[55,20,25,40],[30,6,50,10]], supply=[8,10,11],
 * demand=[4,7,6,12] -> this program reproduces the paper's allocation
 * (x11=4, x14=4, x22=4, x23=6, x32=3, x34=8) and TCC=412 exactly, matching
 * the Python reference implementation (power_rank.py) cell for cell.
 *
 * The paper only defines the method for BALANCED problems (Step 1). This
 * file follows the same convention used for every other method in this
 * project's benchmark: an unbalanced instance is pre-balanced with a
 * zero-cost dummy row (supply<demand) or dummy column (demand<supply)
 * before the algorithm below runs.
 *
 * Input file format (identical to Program_CSM2.cpp / metodeS2new.cpp etc.):
 *   row col
 *   c[0][0] ... c[0][col-1]
 *   ...
 *   c[row-1][0] ... c[row-1][col-1]
 *   s[0] ... s[row-1]
 *   d[0] ... d[col-1]
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#define MAXN 105

char inputName[100];
int row, col;                 /* dimensions AFTER balancing */
double cost[MAXN][MAXN];      /* original costs (never modified) */
double rc[MAXN][MAXN];        /* reduced costs (modified in place) */
double supplyLeft[MAXN], demandLeft[MAXN];
double alloc[MAXN][MAXN];
int rowActive[MAXN], colActive[MAXN];
int nRowActive, nColActive;

void balanceAndRead(FILE *inFile) {
	int i, j, r0, c0;
	double sumS = 0, sumD = 0;
	double s0[MAXN], d0[MAXN];

	fscanf(inFile, "%d %d", &r0, &c0);
	for (i = 0; i < r0; i++)
		for (j = 0; j < c0; j++)
			fscanf(inFile, "%lf", &cost[i][j]);
	for (i = 0; i < r0; i++) { fscanf(inFile, "%lf", &s0[i]); sumS += s0[i]; }
	for (j = 0; j < c0; j++) { fscanf(inFile, "%lf", &d0[j]); sumD += d0[j]; }

	row = r0; col = c0;
	for (i = 0; i < r0; i++) supplyLeft[i] = s0[i];
	for (j = 0; j < c0; j++) demandLeft[j] = d0[j];

	if (sumS < sumD) {                 /* dummy row, cost 0 */
		for (j = 0; j < col; j++) cost[row][j] = 0.0;
		supplyLeft[row] = sumD - sumS;
		row++;
		printf("[Balance] Supply(%.0f) < Demand(%.0f): dummy row added, supply=%.0f\n",
		       sumS, sumD, sumD - sumS);
	} else if (sumD < sumS) {          /* dummy column, cost 0 */
		for (i = 0; i < row; i++) cost[i][col] = 0.0;
		demandLeft[col] = sumS - sumD;
		col++;
		printf("[Balance] Demand(%.0f) < Supply(%.0f): dummy column added, demand=%.0f\n",
		       sumD, sumS, sumS - sumD);
	}

	for (i = 0; i < row; i++)
		for (j = 0; j < col; j++)
			rc[i][j] = cost[i][j];
}

/* Step 2a/2b: ensure every active row, then every active column, has a zero
 * among the still-active cells; only touches rows/columns that lack one. */
void reduceStep(void) {
	int i, j;
	double mn;

	for (i = 0; i < row; i++) {
		if (!rowActive[i]) continue;
		mn = -1;
		for (j = 0; j < col; j++) {
			if (!colActive[j]) continue;
			if (mn < 0 || rc[i][j] < mn) mn = rc[i][j];
		}
		if (mn > 0) {
			for (j = 0; j < col; j++)
				if (colActive[j]) rc[i][j] -= mn;
		}
	}
	for (j = 0; j < col; j++) {
		if (!colActive[j]) continue;
		mn = -1;
		for (i = 0; i < row; i++) {
			if (!rowActive[i]) continue;
			if (mn < 0 || rc[i][j] < mn) mn = rc[i][j];
		}
		if (mn > 0) {
			for (i = 0; i < row; i++)
				if (rowActive[i]) rc[i][j] -= mn;
		}
	}
}

double powerRank(int i, int j) {
	int jj, ii, cnt = 0;
	double sum = 0.0;
	for (jj = 0; jj < col; jj++) {
		if (!colActive[jj] || jj == j) continue;
		if (rc[i][jj] != 0) { sum += rc[i][jj]; cnt++; }
	}
	for (ii = 0; ii < row; ii++) {
		if (!rowActive[ii] || ii == i) continue;
		if (rc[ii][j] != 0) { sum += rc[ii][j]; cnt++; }
	}
	if (cnt == 0) return 0.0;
	return sum / cnt;
}

void compute(void) {
	FILE *inFile = fopen(inputName, "r");
	int i, j, guard = 0;

	if (inFile == NULL) {
		printf("Error! Could not open file\n");
		return;
	}
	balanceAndRead(inFile);
	fclose(inFile);

	nRowActive = row; nColActive = col;
	for (i = 0; i < row; i++) { rowActive[i] = 1; }
	for (j = 0; j < col; j++) { colActive[j] = 1; }
	for (i = 0; i < row; i++) for (j = 0; j < col; j++) alloc[i][j] = 0.0;

	reduceStep(); /* Step 2a */

	while (nRowActive > 0 && nColActive > 0) {
		guard++;
		if (guard > 5000) { printf("Guard tripped - aborting\n"); break; }

		if (nRowActive == 1 || nColActive == 1) {
			for (i = 0; i < row; i++) {
				if (!rowActive[i]) continue;
				for (j = 0; j < col; j++) {
					if (!colActive[j]) continue;
					double q = (supplyLeft[i] < demandLeft[j]) ? supplyLeft[i] : demandLeft[j];
					alloc[i][j] += q;
					supplyLeft[i] -= q;
					demandLeft[j] -= q;
				}
			}
			nRowActive = 0; nColActive = 0;
			break;
		}

		/* Step 3: largest reduced cost among active cells */
		int bi = -1, bj = -1;
		double bestVal = -1;
		for (i = 0; i < row; i++) {
			if (!rowActive[i]) continue;
			for (j = 0; j < col; j++) {
				if (!colActive[j]) continue;
				if (bi < 0 || rc[i][j] > bestVal) { bestVal = rc[i][j]; bi = i; bj = j; }
			}
		}

		/* Step 4: zero candidates in row bi / column bj, scored by power rank */
		int ci = -1, cj = -1;
		double bestRank = -1, bestQty = -1;
		for (j = 0; j < col; j++) {
			if (!colActive[j] || rc[bi][j] != 0) continue;
			double pr = powerRank(bi, j);
			double qty = (supplyLeft[bi] < demandLeft[j]) ? supplyLeft[bi] : demandLeft[j];
			if (pr > bestRank || (pr == bestRank && qty < bestQty)) {
				bestRank = pr; bestQty = qty; ci = bi; cj = j;
			}
		}
		for (i = 0; i < row; i++) {
			if (!rowActive[i] || rc[i][bj] != 0) continue;
			double pr = powerRank(i, bj);
			double qty = (supplyLeft[i] < demandLeft[bj]) ? supplyLeft[i] : demandLeft[bj];
			if (pr > bestRank || (pr == bestRank && qty < bestQty)) {
				bestRank = pr; bestQty = qty; ci = i; cj = bj;
			}
		}

		/* Step 5: allocate */
		double q = (supplyLeft[ci] < demandLeft[cj]) ? supplyLeft[ci] : demandLeft[cj];
		alloc[ci][cj] += q;
		supplyLeft[ci] -= q;
		demandLeft[cj] -= q;
		printf("Allocate %.0f to (%d,%d) [cost %.0f], power rank = %.4f\n",
		       q, ci + 1, cj + 1, cost[ci][cj], bestRank);

		if (supplyLeft[ci] == 0) { rowActive[ci] = 0; nRowActive--; }
		if (demandLeft[cj] == 0) { colActive[cj] = 0; nColActive--; }

		if (nRowActive > 0 && nColActive > 0) reduceStep(); /* Step 6 */
	}

	double TCC = 0;
	for (i = 0; i < row; i++)
		for (j = 0; j < col; j++)
			TCC += cost[i][j] * alloc[i][j];

	printf("\nAllocation matrix:\n");
	for (i = 0; i < row; i++) {
		for (j = 0; j < col; j++) printf("%.0f\t", alloc[i][j]);
		printf("\n");
	}
	printf("\nTCC = %.0f\n", TCC);
}

void custom(void) {
	printf("Name = ");
	scanf("%s", inputName);
}

int main(void) {
	int option = 0;
	printf("Power Rank Method (Kumari, Bose, Gupta & Bala, 2024)\n");
	printf("What is your input-file's name?\n1 = Default (input.txt)\n2 = Custom\n3 = Exit\n");
	printf("Option = ");
	scanf("%d", &option); if (option == 3) return 0;
	do {
		if (option == 1) { strcpy(inputName, "input.txt"); compute(); }
		else if (option == 2) { custom(); compute(); }
		else if (option != 3) printf("Not Valid\n");
		printf("Option = ");
		scanf("%d", &option); if (option == 3);
	} while (option != 3);
	printf("Program Stop\n\n");
	return 0;
}
