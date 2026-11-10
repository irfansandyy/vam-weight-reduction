/*
 * Fractional-Penalty VAM (Fractional VAM / "New Algorithm")
 * -----------------------------------------------------------
 * Direct C++ re-implementation of the MATLAB reference code published in
 * Appendix A of:
 *   D. K. Boah, S. A. Fiele, C. J. Etwire, "A New Algorithm for Finding
 *   Initial Basic Feasible Solutions of Transportation Problems,"
 *   AppliedMath, vol. 6, no. 58, 2026.
 *   (function transportation_solver_newalgorithm_penalty)
 *
 * IMPORTANT (revision note, 2026-09-03): an earlier version of this file
 * reconstructed the algorithm from the paper's PROSE description only and
 * got two things backwards relative to the authors' own MATLAB code
 * (Appendix A), which was re-read after a user-reported mismatch on
 * Example 3 (program gave 9100, paper says 8400):
 *
 *   1. Zero-cost safeguard: when the smallest admissible cost in a row/
 *      column is 0 (e.g. a dummy cell), the MATLAB code sets that line's
 *      penalty to 0 (LOW priority, deprioritised) -- not a very large
 *      sentinel as this file previously assumed.
 *   2. A line with only ONE remaining admissible cell gets a penalty equal
 *      to that cell's cost itself (not 0 and not "no penalty").
 *
 * Both fixes are taken directly from Appendix A's MATLAB listing and are
 * unambiguous.
 *
 * Row-vs-column tie-break -- the paper is INTERNALLY INCONSISTENT here:
 *   - Appendix A's MATLAB code prefers the COLUMN on a tie
 *     (`if max_cp > max_rp || (max_cp == max_rp && max_cp ~= -inf)`).
 *   - Section 4.3's worked demonstration (the BUA Cement Company example,
 *     which is Example 28 in Tables 3-8) says in prose: "A tie occurs
 *     between row P1 and column D2. Using the deterministic row-first
 *     rule, select row P1."
 *   These two parts of the same paper contradict each other. Empirically,
 *   against all 34 published benchmark costs: column-first reproduces
 *   30/34 (fails Examples 12, 17, 24, 28); row-first reproduces 31/34
 *   (fails Examples 3, 17, 24). This file uses ROW-FIRST because (a) it
 *   matches the paper's own explicit worked demonstration in words, and
 *   (b) it has the higher match rate. Examples 3, 17 and 24 still do not
 *   reproduce the paper's published "New Algorithm" cost with either
 *   convention -- see dataset-benchmark-fractional-vam-34-instance.md /
 *   kode-cpp-modified-lcm-fractional-vam.md in the project for the full
 *   comparison table and an open question about what else might differ.
 *
 * General steps (Section 3.2 / Appendix A of the paper):
 *   1. Balance the problem (dummy row/column with zero cost if needed).
 *   2. While supply and demand remain:
 *        a. For every row with remaining supply, compute the penalty from
 *           its two smallest costs among columns with remaining demand
 *           (ratio m(2)/m(1), or 0 if m(1)=0, or the cost itself if only
 *           one admissible column remains).
 *        b. Same for every column with remaining demand.
 *        c. If the best column penalty is >= the best row penalty (ties go
 *           to the column), select that column and the row with the
 *           smallest cost in it; otherwise select the best row and the
 *           column with the smallest cost in it.
 *        d. Allocate min(remaining supply, remaining demand) at that cell;
 *           update the rim; a row/column that reaches zero is excluded
 *           from all further consideration (cost treated as +inf).
 *   3. Total cost = sum of (allocated units x unit cost).
 *
 * This program only constructs the Initial Basic Feasible Solution (IBFS);
 * it does not run MODI / Stepping-Stone optimisation.
 *
 * Input file format (identical to the RBSM/CSM programs used in this
 * project, e.g. metodeS2new.cpp):
 *   row col
 *   c[0][0] c[0][1] ... c[0][col-1]
 *   ...
 *   c[row-1][0] ... c[row-1][col-1]
 *   s[0] s[1] ... s[row-1]
 *   d[0] d[1] ... d[col-1]
 */

#include <cstdio>
#include <cstring>
#include <climits>
#include <limits>

const int MAXN = 205;

char inputName[100];
int origRow, origCol;
int row, col;
long long costMat[MAXN][MAXN];
long long sLeft[MAXN], dLeft[MAXN];
long long allocMat[MAXN][MAXN];
bool rowActive[MAXN], colActive[MAXN];

const double NEG_INF = -std::numeric_limits<double>::infinity();

void readInput(FILE *inFile) {
	fscanf(inFile, "%d %d", &origRow, &origCol);
	row = origRow; col = origCol;

	for (int i = 0; i < origRow; i++)
		for (int j = 0; j < origCol; j++)
			fscanf(inFile, "%lld", &costMat[i][j]);

	for (int i = 0; i < origRow; i++)
		fscanf(inFile, "%lld", &sLeft[i]);

	for (int j = 0; j < origCol; j++)
		fscanf(inFile, "%lld", &dLeft[j]);
}

void balance() {
	long long sumS = 0, sumD = 0;
	for (int i = 0; i < row; i++) sumS += sLeft[i];
	for (int j = 0; j < col; j++) sumD += dLeft[j];

	if (sumS > sumD) {
		for (int i = 0; i < row; i++) costMat[i][col] = 0;
		dLeft[col] = sumS - sumD;
		col++;
		printf("[Balance] Supply(%lld) > Demand(%lld): dummy column added, demand=%lld\n",
			sumS, sumD, sumS - sumD);
	} else if (sumD > sumS) {
		for (int j = 0; j < col; j++) costMat[row][j] = 0;
		sLeft[row] = sumD - sumS;
		row++;
		printf("[Balance] Demand(%lld) > Supply(%lld): dummy row added, supply=%lld\n",
			sumD, sumS, sumD - sumS);
	} else {
		printf("[Balance] Problem already balanced (Supply = Demand = %lld)\n", sumS);
	}
}

// Count of active columns in row i, plus smallest (m1) and second-smallest
// (m2, only meaningful if count>=2) cost among them.
int rowValidCount(int i, long long &m1, long long &m2) {
	m1 = m2 = LLONG_MAX;
	int count = 0;
	for (int j = 0; j < col; j++) {
		if (!colActive[j]) continue;
		count++;
		long long c = costMat[i][j];
		if (c < m1) { m2 = m1; m1 = c; }
		else if (c < m2) { m2 = c; }
	}
	return count;
}

int colValidCount(int j, long long &m1, long long &m2) {
	m1 = m2 = LLONG_MAX;
	int count = 0;
	for (int i = 0; i < row; i++) {
		if (!rowActive[i]) continue;
		count++;
		long long c = costMat[i][j];
		if (c < m1) { m2 = m1; m1 = c; }
		else if (c < m2) { m2 = c; }
	}
	return count;
}

// Penalty per Appendix A: ratio m2/m1 (0 if m1==0) when >=2 admissible
// cells remain; the cost itself when exactly 1 remains; -inf otherwise.
double penaltyFromCounts(int count, long long m1, long long m2) {
	if (count >= 2) {
		if (m1 == 0) return 0.0;
		return (double) m2 / (double) m1;
	} else if (count == 1) {
		return (double) m1;
	}
	return NEG_INF;
}

// Within column c, the row with the smallest cost among active rows
// (tie-break: smallest row index, matching MATLAB's min()).
int cheapestActiveRowInCol(int c) {
	int best = -1;
	for (int i = 0; i < row; i++) {
		if (!rowActive[i]) continue;
		if (best == -1 || costMat[i][c] < costMat[best][c]) best = i;
	}
	return best;
}

// Within row r, the column with the smallest cost among active columns
// (tie-break: smallest column index).
int cheapestActiveColInRow(int r) {
	int best = -1;
	for (int j = 0; j < col; j++) {
		if (!colActive[j]) continue;
		if (best == -1 || costMat[r][j] < costMat[r][best]) best = j;
	}
	return best;
}

void printAllocation() {
	printf("\nAllocation matrix (cost[allocated]):\n");
	for (int i = 0; i < row; i++) {
		for (int j = 0; j < col; j++)
			printf("%4lld[%lld]\t", costMat[i][j], allocMat[i][j]);
		printf("\n");
	}
}

void compute() {
	FILE *inFile = fopen(inputName, "r");
	if (inFile == NULL) {
		printf("Error! Could not open file\n");
		return;
	}
	memset(costMat, 0, sizeof(costMat));
	memset(allocMat, 0, sizeof(allocMat));

	readInput(inFile);
	fclose(inFile);

	balance();

	for (int i = 0; i < row; i++) rowActive[i] = (sLeft[i] > 0);
	for (int j = 0; j < col; j++) colActive[j] = (dLeft[j] > 0);

	int iteration = 1;
	while (true) {
		bool anyS = false, anyD = false;
		for (int i = 0; i < row; i++) if (sLeft[i] > 0) { anyS = true; break; }
		for (int j = 0; j < col; j++) if (dLeft[j] > 0) { anyD = true; break; }
		if (!anyS || !anyD) break;

		double bestRP = NEG_INF; int rStar = -1;
		for (int i = 0; i < row; i++) {
			if (sLeft[i] <= 0) continue;
			long long m1, m2;
			int cnt = rowValidCount(i, m1, m2);
			double p = penaltyFromCounts(cnt, m1, m2);
			if (p > bestRP) { bestRP = p; rStar = i; }
		}

		double bestCP = NEG_INF; int cStar = -1;
		for (int j = 0; j < col; j++) {
			if (dLeft[j] <= 0) continue;
			long long m1, m2;
			int cnt = colValidCount(j, m1, m2);
			double p = penaltyFromCounts(cnt, m1, m2);
			if (p > bestCP) { bestCP = p; cStar = j; }
		}

		// Tie-break: ROW-FIRST (see revision note above -- the paper's own
		// Section 4.3 demonstration states this rule explicitly in words,
		// even though Appendix A's MATLAB code implements column-first).
		int chosenRow = -1, chosenCol = -1;
		bool chosenIsCol;
		if (bestCP > bestRP) {
			chosenCol = cStar;
			chosenRow = cheapestActiveRowInCol(chosenCol);
			chosenIsCol = true;
		} else if (bestRP != NEG_INF) {
			chosenRow = rStar;
			chosenCol = cheapestActiveColInRow(chosenRow);
			chosenIsCol = false;
		} else {
			break; // no valid allocations left (should not happen)
		}

		long long amount = sLeft[chosenRow] < dLeft[chosenCol] ? sLeft[chosenRow] : dLeft[chosenCol];
		allocMat[chosenRow][chosenCol] += amount;
		sLeft[chosenRow] -= amount;
		dLeft[chosenCol] -= amount;

		printf("Iteration %d: %s %d selected (row_p=%.4f, col_p=%.4f) -> allocate %lld unit(s) at (row %d, col %d), cost %lld\n",
			iteration++, chosenIsCol ? "col" : "row", (chosenIsCol ? chosenCol : chosenRow) + 1,
			bestRP, bestCP, amount, chosenRow + 1, chosenCol + 1, costMat[chosenRow][chosenCol]);

		if (sLeft[chosenRow] == 0) rowActive[chosenRow] = false;
		if (dLeft[chosenCol] == 0) colActive[chosenCol] = false;
	}

	printAllocation();

	long long TCC = 0;
	for (int i = 0; i < row; i++)
		for (int j = 0; j < col; j++)
			TCC += costMat[i][j] * allocMat[i][j];
	printf("\nTCC = %lld\n", TCC);

	FILE *outFile = fopen("output_fractional_vam.txt", "w");
	if (outFile) {
		fprintf(outFile, "Method: Fractional VAM / New Algorithm (Boah, Fiele & Etwire, 2026)\n");
		fprintf(outFile, "Rows=%d Cols=%d (after balancing)\n", row, col);
		for (int i = 0; i < row; i++) {
			for (int j = 0; j < col; j++)
				fprintf(outFile, "%lld ", allocMat[i][j]);
			fprintf(outFile, "\n");
		}
		fprintf(outFile, "TCC = %lld\n", TCC);
		fclose(outFile);
	}
}

void custom() {
	printf("Name = ");
	scanf("%s", inputName);
}

int main() {
	int option = 0;
	printf("Fractional VAM / New Algorithm (Boah, Fiele & Etwire, 2026)\n");
	printf("What is your input-file's name?\n1 = Default (input.txt)\n2 = Custom\n3 = Exit\n");
	printf("Option = ");
	scanf("%d", &option);
	if (option == 3) return 0;
	do {
		if (option == 1) { strcpy(inputName, "input.txt"); compute(); }
		else if (option == 2) { custom(); compute(); }
		else if (option != 3) printf("Not Valid\n");
		printf("Option = ");
		scanf("%d", &option);
	} while (option != 3);
	printf("Program Stop\n\n");
	return 0;
}
