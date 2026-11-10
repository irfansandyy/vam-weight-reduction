
/* BENCHMARK ADAPTER: quiet CLI I/O, one instance, allocation output and
 * undefined-index/iteration guards only. Original heuristic rules unchanged. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define printf(...) ((void)0)
static unsigned long adapter_steps = 0;
static void adapter_fail(const char *why) {
    fprintf(stderr, "__FAIL__ %s\n", why); exit(65);
}
static void adapter_guard(void) {
    if (++adapter_steps > 100000UL) adapter_fail("iteration_guard");
}
static void adapter_emit_int(int m, int n, const int *a) {
    fprintf(stdout, "__RESULT__ %d %d\n", m, n);
    for (int r = 0; r < m; r++) {
        for (int c = 0; c < n; c++) {
            int q = a[r*n+c];
            fprintf(stdout, "%d%c", q == -1 ? 0 : q, c+1 == n ? '\n' : ' ');
        }
    }
}
#include<stdio.h>
#include<stdlib.h>

int supply[2000];
int demand[2000];
int max_allocation(int, int);
int count =1;

int main(int argc, char **argv)
{
	int i, j, jumlah_supply, jumlah_demand, flag; 
	char dummy;	
	
	FILE *in;
	if (argc != 2) return 64;
	in=fopen(argv[1],"r");
	if (!in) return 66;
	
	for (int adapter_once=0; adapter_once<1; adapter_once++)
	{
		
		fscanf(in,"%d", &jumlah_supply);	
		fscanf(in,"%d", &jumlah_demand);
	
		int cost[jumlah_supply][jumlah_demand];
		int v_cost[jumlah_supply][jumlah_demand];
		int h_cost[jumlah_supply][jumlah_demand];
		int tocm_cost[jumlah_supply][jumlah_demand];
		int allocation[jumlah_supply][jumlah_demand];
		int row_status[jumlah_supply];
		
		// input cost
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				fscanf(in,"%d",&cost[i][j]);
			}
		}
		
		// input supply
		for(i=0;i<jumlah_supply;i++)
		{
			fscanf(in,"%d",&supply[i]);
		}
		
		// input demand
		for(i=0;i<jumlah_demand;i++)
		{
			fscanf(in,"%d",&demand[i]);
		}
		
		// menghitung total supply dan demand
		int total_demand=0, total_supply=0;
		for(i=0;i<jumlah_supply;i++) total_supply+=supply[i];
		for(j=0;j<jumlah_demand;j++) total_demand+=demand[j];
		
		// mengeset semua alokasi dan row_status menjadi -1
		for(i=0;i<jumlah_supply;i++)
		{
			row_status[i]=-1;
			for(j=0;j<jumlah_demand;j++)
			{
				allocation[i][j]=-1;
			}
		}
		
		// mencari cost terkecil tiap baris
		int h_min[jumlah_supply];
		for(i=0;i<jumlah_supply;i++)
		{
			h_min[i]=99999;
			for(j=0;j<jumlah_demand;j++)
			{
				if(cost[i][j]<h_min[i]) h_min[i]=cost[i][j];
			}
		}
		
		// mencari cost terkecil tiap kolom
		int v_min[jumlah_demand];
		for(j=0;j<jumlah_demand;j++)
		{
			v_min[j]=99999;
			for(i=0;i<jumlah_supply;i++)
			{
				if(cost[i][j]<v_min[j]) v_min[j]=cost[i][j];
			}
		}
		
		// menentukan h_cost
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				h_cost[i][j]=cost[i][j]-h_min[i];
			}
		}
		
		// menentukan v_cost
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				v_cost[i][j]=cost[i][j]-v_min[j];
			}
		}
		
		// menghitung matrix TOCM
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				tocm_cost[i][j]=v_cost[i][j]+h_cost[i][j];
			}
		}		
		
		// print matrix TOCM
		printf("\n\n\n");
		printf("matrix TOCM\n");
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				printf("%d\t",tocm_cost[i][j]);
			}
			printf("\n");
		}
//		dummy=getchar();
//		dummy=getchar();
		
		while(1)
		{
			adapter_guard();
			printf("=== ITERASI KE-%d ===\n", count++);
			if(total_supply!=total_demand)
			{
				printf("\nsoal unbalance\n");
				break;
			}
			
			// mencari minimal cost tiap baris pada matrix TOCM
			int min[jumlah_supply];
			int min_position[jumlah_supply];
			for(i=0;i<jumlah_supply;i++)
			{
				min[i]=99999;
				if(row_status[i]==-1)
				{
					for(j=0;j<jumlah_demand;j++)
					{
						if(tocm_cost[i][j]<min[i] && allocation[i][j]==-1) 
						{
							min[i]=tocm_cost[i][j];
							min_position[i]=j;
						}
						else if(tocm_cost[i][j]==min[i] && allocation[i][j]==-1)
						{
							if(max_allocation(i,j) > max_allocation(i, min_position[i]))
							{
								min[i]=tocm_cost[i][j];
								min_position[i]=j;						
							}					
						}	
					}
				}
			}
			
			// menghitung penalty tiap baris
			int penalty[jumlah_supply];
			for(i=0;i<jumlah_supply;i++)
			{
				penalty[i]=0;
				if(row_status[i]==-1)
				{
					for(j=0;j<jumlah_demand;j++)
					{
						if(allocation[i][j]==-1) penalty[i]+=tocm_cost[i][j]-min[i];
					}
				}
			}
			
			
			// mencari penalty terbesar
			int chosen_row, max=-1;
			for(i=0;i<jumlah_supply;i++)
			{
				if(penalty[i]>max && row_status[i]==-1)
				{
					max=penalty[i];
					chosen_row=i;
				}
				else if(penalty[i]==max && row_status[i]==-1)
				{
					if(min[i]<min[chosen_row])
					{
						max=penalty[i];
						chosen_row=i;				
					}
					else if(min[i]==min[chosen_row])
					{
						int total1=0, total2=0;
						for(j=0;j<jumlah_demand;j++)
						{
							total1+=tocm_cost[i][j];
							total2+=tocm_cost[chosen_row][j];
						}
						if(total1>total2)
						{
							max=penalty[i];
							chosen_row=i;
						}
						else if (total1==total2)
						{
							if(max_allocation(i, min_position[i]) > max_allocation(chosen_row, min_position[chosen_row]))
							{
								max=penalty[i];
								chosen_row=i;						
							}
						}
						
					}
				}
			}
			
			printf("\n\nmax penalty baris ke-%d",chosen_row);
			
			// mencari next penalty
			int next_row=-1; max=-1;
			for(i=0;i<jumlah_supply;i++)
			{
				if(penalty[i]>max && i!=chosen_row && row_status[i]==-1)
				{
					max=penalty[i];
					next_row=i;
				}
				else if(penalty[i]==max && i!=chosen_row && row_status[i]==-1)
				{
					if(min[i]<min[next_row])
					{
						max=penalty[i];
						next_row=i;				
					}
					else if(min[i]==min[next_row])
					{
						int total1=0, total2=0;
						for(j=0;j<jumlah_demand;j++)
						{
							total1+=tocm_cost[i][j];
							total2+=tocm_cost[next_row][j];
						}
						if(total1>total2)
						{
							max=penalty[i];
							next_row=i;
						}
						else if (total1==total2)
						{
							if(max_allocation(i, min_position[i]) > max_allocation(next_row, min_position[next_row]))
							{
								max=penalty[i];
								next_row=i;						
							}
						}
						
					}
				}
			}
			if (next_row!=-1) printf("\nnext penalty baris ke-%d",next_row);
			else printf("\ntidak ada next-row\n");
			
			// apabila minimal cost chosen row <> 0
			if(tocm_cost[chosen_row][min_position[chosen_row]]!=0)
			{
				allocation[chosen_row][min_position[chosen_row]]=max_allocation(chosen_row,min_position[chosen_row]);
				supply[chosen_row]-=allocation[chosen_row][min_position[chosen_row]];
				demand[min_position[chosen_row]]-=allocation[chosen_row][min_position[chosen_row]];
			}
			
			// apabila minimal cost chosen row = 0
			else if(next_row!=-1)
			{
				int menang_now=0, menang_next=0;
				for(j=0;j<jumlah_demand;j++)
				{
					if(allocation[chosen_row][j]==-1) 
					{
						if(tocm_cost[chosen_row][j]>=tocm_cost[next_row][j]) menang_now++;
						else if(tocm_cost[chosen_row][j]<tocm_cost[next_row][j]) menang_next++;
					}
				}
				
				if (menang_next>menang_now)
				{
					allocation[next_row][min_position[next_row]]=max_allocation(next_row,min_position[next_row]);
					supply[next_row]-=allocation[next_row][min_position[next_row]];
					demand[min_position[next_row]]-=allocation[next_row][min_position[next_row]];
				}
				
				else
				{
					allocation[chosen_row][min_position[chosen_row]]=max_allocation(chosen_row,min_position[chosen_row]);
					supply[chosen_row]-=allocation[chosen_row][min_position[chosen_row]];
					demand[min_position[chosen_row]]-=allocation[chosen_row][min_position[chosen_row]];	
				}
				
			}
			
			// apabila sudah tidak ada next row
			else
			{
				allocation[chosen_row][min_position[chosen_row]]=max_allocation(chosen_row,min_position[chosen_row]);
				supply[chosen_row]-=allocation[chosen_row][min_position[chosen_row]];
				demand[min_position[chosen_row]]-=allocation[chosen_row][min_position[chosen_row]];	
			}
			
			// satisfy row
			int end=0;
			for(i=0;i<jumlah_supply;i++)
			{
				row_status[i]=-1;
				if(supply[i]==0)
				{
					for(j=0;j<jumlah_demand;j++)
					{
						if(allocation[i][j]==-1) allocation[i][j]=0;
					}
					row_status[i]=0;
				}
				if (row_status[i]==-1) end=-1; 
			}
			
			// satisfy kolom
			for(j=0;j<jumlah_demand;j++)
			{
				if(demand[j]==0)
				{
					for(i=0;i<jumlah_supply;i++)
					{
						if(allocation[i][j]==-1) allocation[i][j]=0;
					}
				}
			}
			
			
			// print alokasi
			printf("\n\n\n");
			printf("ALOKASI\n");
			for(i=0;i<jumlah_supply;i++)
			{
				for(j=0;j<jumlah_demand;j++)
				{
					if(allocation[i][j]!=-1) printf("%d\t",allocation[i][j]);
					else printf("X\t");
				}
				printf("-- %d\n",supply[i]);
			}
			printf("\n");
			for(j=0;j<jumlah_demand;j++) printf("%d\t",demand[j]);
//			dummy=getchar();
//			dummy=getchar();
			
			
			// melakukan looping
			if (end==0) break;
		}
		
		// menghitung total
		adapter_emit_int(jumlah_supply, jumlah_demand, &allocation[0][0]);
		long long result=0;
		if(total_supply==total_demand)
		{
			for(i=0;i<jumlah_supply;i++)
			{
				for(j=0;j<jumlah_demand;j++) result+=(1LL*allocation[i][j]*cost[i][j]);
			}
			printf("\n\n------akhir-------\n");
			printf("total biaya = %d\n------------------\n", result);	
			break;

//			int closing;
//			scanf("%d",&closing);
		}
		
		
	}
	
//	system("pause");
	return 0;
}


int max_allocation(int i,int j)
{
	if(supply[i]<demand[j]) return supply[i];
	else return demand[j];
}
