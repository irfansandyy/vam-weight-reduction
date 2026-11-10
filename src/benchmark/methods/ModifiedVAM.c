/*
 * VAM Reduksi Berbobot (VRB), constructive heuristic, no optimization phase.
 *
 * Build: gcc -O2 -std=c11 -Wall -Wextra ModifiedVAM.c -o ModifiedVAM
 * Run:   ./ModifiedVAM input.txt [--json] [--trace]
 *
 * Input: m n; m*n nonnegative integer costs; m supplies; n demands.
 * Optional trailing reference values are ignored. Zero marginals are allowed.
 * Unbalanced inputs receive one zero-cost dummy row/column.
 * Dimensions are limited to 127 before the optional dummy is appended.
 * Arithmetic uses checked signed 64-bit integers and rejects overflow.
 *
 * Reduce each row once, then each column once. Run the existing weighted
 * VAM score on the reduced costs: (SLC-FLC)*(MAX+TLC-FLC).
 * FLC (First Least Cost), SLC (Second Least Cost), and TLC (Third Least Cost)
 * are the three smallest ACTIVE costs, counting duplicate values.
 * MAX is the maximum active cost in the line.
 * penalty = SLC-FLC; weight = MAX+TLC-FLC; score = penalty*weight.
 * If there are two active costs, set TLC=SLC. If there is one, its allocation
 * is forced and its score is zero. The total uses ORIGINAL costs.
 *
 * Within a line: minimum reduced cost, maximum feasible quantity, index.
 * Across lines: maximum score, minimum reduced candidate cost, maximum
 * feasible quantity, rows before columns, then ascending line index.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <limits.h>

#define MAX_DIMENSION 128
typedef int64_t I;
static int m,n,original_m,original_n;
static I original[MAX_DIMENSION][MAX_DIMENSION], reduced[MAX_DIMENSION][MAX_DIMENSION];
static I allocation[MAX_DIMENSION][MAX_DIMENSION];
static I supply[MAX_DIMENSION],demand[MAX_DIMENSION];
static I initial_supply[MAX_DIMENSION],initial_demand[MAX_DIMENSION];
static I row_reduction[MAX_DIMENSION],column_reduction[MAX_DIMENSION];
static int json_mode,trace_mode;
typedef struct {
    int valid,axis,index,i,j,count;
    I FLC,SLC,TLC,MAX;
    I penalty,weight,score,quantity;
} Line;

static void fail(const char *message) {
    fprintf(stderr,"VRB: %s\n",message);
    exit(EXIT_FAILURE);
}
static I add(I left,I right) {
    I result;
    if (__builtin_add_overflow(left,right,&result)) fail("integer addition overflow");
    return result;
}
static I mul(I left,I right) {
    I result;
    if (__builtin_mul_overflow(left,right,&result)) fail("integer multiplication overflow");
    return result;
}
static I read_integer(FILE *file) {
    char token[128],*end;
    if (fscanf(file,"%127s",token)!=1) fail("missing input value");
    errno=0;
    intmax_t value=strtoimax(token,&end,10);
    if (errno==ERANGE || *end || value<0 || value>INT64_MAX)
        fail("input must contain nonnegative signed 64-bit integers");
    return (I)value;
}
static I smaller(I left,I right) { return left<right?left:right; }

static void load(const char *path) {
    FILE *file=fopen(path,"r");
    if (!file) fail("cannot open input file");
    I rows=read_integer(file),columns=read_integer(file);
    if (rows<1 || columns<1 || rows>=MAX_DIMENSION || columns>=MAX_DIMENSION)
        fail("dimensions must be between 1 and 127");
    m=original_m=(int)rows;n=original_n=(int)columns;
    for (int i=0;i<m;i++) for (int j=0;j<n;j++)
        original[i][j]=read_integer(file);
    I total_supply=0,total_demand=0;
    for (int i=0;i<m;i++) {
        supply[i]=read_integer(file);
        total_supply=add(total_supply,supply[i]);
    }
    for (int j=0;j<n;j++) {
        demand[j]=read_integer(file);
        total_demand=add(total_demand,demand[j]);
    }
    fclose(file);
    if (total_supply>total_demand) demand[n++]=total_supply-total_demand;
    else if (total_demand>total_supply) supply[m++]=total_demand-total_supply;
    memcpy(initial_supply,supply,sizeof supply);
    memcpy(initial_demand,demand,sizeof demand);
    memcpy(reduced,original,sizeof original);
    /* Rows and columns with zero marginals cannot affect the feasible plan. */
    for (int i=0;i<m;i++) if (supply[i]>0) {
        I minimum=0;int found=0;
        for (int j=0;j<n;j++) if (demand[j]>0 && (!found || reduced[i][j]<minimum)) {
            minimum=reduced[i][j];found=1;
        }
        row_reduction[i]=minimum;
        for (int j=0;j<n;j++) if (demand[j]>0) reduced[i][j]-=minimum;
    }
    for (int j=0;j<n;j++) if (demand[j]>0) {
        I minimum=0;int found=0;
        for (int i=0;i<m;i++) if (supply[i]>0 && (!found || reduced[i][j]<minimum)) {
            minimum=reduced[i][j];found=1;
        }
        column_reduction[j]=minimum;
        for (int i=0;i<m;i++) if (supply[i]>0) reduced[i][j]-=minimum;
    }
}

static Line inspect_line(int axis,int index) {
    Line line={0};line.axis=axis;line.index=index;
    if ((axis?demand[index]:supply[index])==0) return line;
    line.FLC=line.SLC=line.TLC=INT64_MAX;
    int position=-1;
    for (int p=0;p<(axis?m:n);p++) {
        int i=axis?p:index,j=axis?index:p;
        if (supply[i]==0 || demand[j]==0) continue;
        I value=reduced[i][j],quantity=smaller(supply[i],demand[j]);
        line.count++;
        if (value>line.MAX) line.MAX=value;
        if (position<0 || value<line.FLC) {
            line.TLC=line.SLC;line.SLC=line.FLC;line.FLC=value;
            position=p;line.quantity=quantity;
        } else {
            if (value==line.FLC && quantity>line.quantity) {
                position=p;line.quantity=quantity;
            }
            if (value<line.SLC) { line.TLC=line.SLC;line.SLC=value; }
            else if (value<line.TLC) line.TLC=value;
        }
    }
    if (position<0) return line;
    line.valid=1;
    line.i=axis?position:index;line.j=axis?index:position;
    if (line.count==1) line.SLC=line.TLC=line.FLC;
    else if (line.count==2) line.TLC=line.SLC;
    line.penalty=line.SLC-line.FLC;
    line.weight=add(line.MAX,line.TLC-line.FLC);
    line.score=mul(line.penalty,line.weight);
    return line;
}
static int is_better(Line candidate,Line best) {
    if (!candidate.valid) return 0;
    if (!best.valid) return 1;
    if (candidate.score!=best.score) return candidate.score>best.score;
    if (candidate.FLC!=best.FLC) return candidate.FLC<best.FLC;
    if (candidate.quantity!=best.quantity) return candidate.quantity>best.quantity;
    if (candidate.axis!=best.axis) return candidate.axis<best.axis;
    return candidate.index<best.index;
}
static void print_vector(const I *values,int length) {
    putchar('[');
    for (int i=0;i<length;i++) printf("%s%" PRId64,i?",":"",values[i]);
    putchar(']');
}
static void print_matrix(I values[MAX_DIMENSION][MAX_DIMENSION]) {
    putchar('[');
    for (int i=0;i<m;i++) {
        if (i) putchar(',');
        print_vector(values[i],n);
    }
    putchar(']');
}
static void print_line(Line line) {
    printf("{\"axis\":\"%s\",\"index\":%d,\"active_count\":%d,"
           "\"FLC\":%" PRId64 ",\"SLC\":%" PRId64 ",\"TLC\":%" PRId64
           ",\"MAX\":%" PRId64 ",\"penalty\":%" PRId64
           ",\"weight\":%" PRId64 ",\"score\":%" PRId64
           ",\"row\":%d,\"column\":%d,\"quantity\":%" PRId64 "}",
           line.axis?"column":"row",line.index+1,line.count,
           line.FLC,line.SLC,line.TLC,line.MAX,line.penalty,line.weight,line.score,
           line.i+1,line.j+1,line.quantity);
}
static void verify(I expected_total) {
    I total=0;
    for (int i=0;i<m;i++) {
        I sum=0;
        for (int j=0;j<n;j++) {
            if (allocation[i][j]<0) fail("negative allocation");
            sum=add(sum,allocation[i][j]);
            total=add(total,mul(allocation[i][j],original[i][j]));
        }
        if (sum!=initial_supply[i]) fail("row feasibility check failed");
    }
    for (int j=0;j<n;j++) {
        I sum=0;
        for (int i=0;i<m;i++) sum=add(sum,allocation[i][j]);
        if (sum!=initial_demand[j]) fail("column feasibility check failed");
    }
    if (total!=expected_total) fail("total cost check failed");
}

int main(int argc,char **argv) {
    if (argc<2) {
        fprintf(stderr,"Usage: %s input.txt [--json] [--trace]\n",argv[0]);
        return EXIT_FAILURE;
    }
    for (int argument_index=2;argument_index<argc;argument_index++) {
        if (strcmp(argv[argument_index],"--json")==0) json_mode=1;
        else if (strcmp(argv[argument_index],"--trace")==0) trace_mode=1;
        else fail("unknown option");
    }
    load(argv[1]);
    if (json_mode) {
        printf("{\"method\":\"VRB\",\"rows\":%d,\"columns\":%d,"
               "\"original_rows\":%d,\"original_columns\":%d,\"row_reductions\":",
               m,n,original_m,original_n);
        print_vector(row_reduction,m);
        printf(",\"column_reductions\":");print_vector(column_reduction,n);
        printf(",\"reduced_costs\":");print_matrix(reduced);
        if (trace_mode) printf(",\"trace\":[");
    }
    I remaining=0,total=0;
    for (int i=0;i<m;i++) remaining=add(remaining,supply[i]);
    int iteration=0;
    while (remaining>0) {
        Line best={0},lines[MAX_DIMENSION*2];int line_count=0;
        for (int axis=0;axis<2;axis++) for (int line_index=0;line_index<(axis?n:m);line_index++) {
            Line line=inspect_line(axis,line_index);
            if (line.valid) {
                lines[line_count++]=line;
                if (is_better(line,best)) best=line;
            }
        }
        if (!best.valid || best.quantity<=0) fail("cannot find a positive feasible allocation");
        iteration++;
        I step_cost=mul(best.quantity,original[best.i][best.j]);
        total=add(total,step_cost);
        if (trace_mode && json_mode) {
            printf("%s{\"iteration\":%d,\"supply_before\":",iteration>1?",":"",iteration);
            print_vector(supply,m);printf(",\"demand_before\":");print_vector(demand,n);
            printf(",\"lines\":[");
            for (int line_index=0;line_index<line_count;line_index++) {
                if (line_index) putchar(',');
                print_line(lines[line_index]);
            }
            printf("],\"selected\":");print_line(best);
        } else if (trace_mode) {
            printf("Iterasi %d: %s %d, skor=%" PRId64 ", sel=(%d,%d), alokasi=%" PRId64 ", biaya=%" PRId64 "\n",
                   iteration,best.axis?"kolom":"baris",best.index+1,best.score,
                   best.i+1,best.j+1,best.quantity,step_cost);
        }
        allocation[best.i][best.j]=add(allocation[best.i][best.j],best.quantity);
        supply[best.i]-=best.quantity;demand[best.j]-=best.quantity;
        remaining-=best.quantity;
        if (trace_mode && json_mode) {
            printf(",\"supply_after\":");print_vector(supply,m);
            printf(",\"demand_after\":");print_vector(demand,n);
            printf(",\"step_cost\":%" PRId64 ",\"cumulative_cost\":%" PRId64 "}",step_cost,total);
        }
    }
    verify(total);
    if (json_mode) {
        if (trace_mode) putchar(']');
        printf(",\"iterations\":%d,\"cost\":%" PRId64 ",\"allocation\":",iteration,total);
        print_matrix(allocation);printf(",\"valid\":true}\n");
    } else {
        printf("Metode: VAM Reduksi Berbobot (VRB)\nIterasi: %d\nTotal biaya: %" PRId64 "\n",iteration,total);
        for (int i=0;i<m;i++) {
            for (int j=0;j<n;j++) printf("%s%" PRId64,j?" ":"",allocation[i][j]);
            putchar('\n');
        }
    }
    return EXIT_SUCCESS;
}
