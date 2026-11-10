#include<stdio.h>
#include<stdlib.h>

int jumlah_supply, jumlah_demand;
int total_supply=0, total_demand=0;
int cost[100][100];
int allocation[100][100], total_allocation[100];
int supply_awal[100];
int supply[100], demand[100];	
int min, min_pos[100]; 
int row_status[100]; 
int leastPosition[100], secondPosition[100];
int min_diff, diff[100]; 
int chosen_i, chosen_j;
int i, j, flag, p, q, balanced;
int count =1;
char dummy;

int satisfy_least();
int satisfy_second();
int drop_all_to_second();
int print_state(int);
int totalize_allocation_at_row(int);
int initiate_allocation();
int totalize_allocation_at_row(int);
int cross_row(int);
int print_result();

int main()
{		

	FILE *in;
	in=fopen("s4.txt","r");
	
	while(!feof(in))
	{
		fscanf(in,"%d", &jumlah_supply);	
		fscanf(in,"%d", &jumlah_demand);
	
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
			total_supply+=supply[i];
			supply_awal[i]=supply[i];
		}
		
		// input demand
		for(i=0;i<jumlah_demand;i++)
		{
			fscanf(in,"%d",&demand[i]);
			total_demand+=demand[i];
		}
		
		//mengeset alokasi semua cell menjadi -1
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				allocation[i][j]=-1;
			}
		}
		
		// menjustifikasi balanced atau unbalanced
		if (total_supply==total_demand) balanced=1;
		else balanced=0;
		
		// mengeprint state
		print_state(0);
		
		//menginisiasi, mengalokasikan demand kepada cost terkecil tiap kolom		
		initiate_allocation();
		
		// menjustifikasi unmet setiap row
		// row status :
		// 1 = unmet ( total demand > supply )
		// 2 = tidak unmet ( total demand < supply )	
		for(i=0;i<jumlah_supply;i++)
		{
			totalize_allocation_at_row(i);
			if (supply[i]<0) row_status[i]=1;
			else row_status[i]=2;
		}
		
		
		while(1)
		{			
			//mencari least, second, dan diff tiap cell pada unmet row		
			min_diff=999999;
			for(j=0;j<jumlah_demand;j++)
			{
				// mencari least
				leastPosition[j]=-1;
				for(i=0;i<jumlah_supply;i++)
				{
					if(allocation[i][j]>0 && row_status[i]==1) leastPosition[j]=i;
				}
				
				// mencari second
				secondPosition[j]=-1;
				if(leastPosition[j]>-1) 
				{
					min=9999999;
					for(i=0;i<jumlah_supply;i++)
					{
						if(cost[i][j]<min && allocation[i][j]==-1) 
						{
							min=cost[i][j];
							secondPosition[j]=i;
						}		
					}
					
					// mencari diff tiap kolom dan diff terkecil
					diff[j]=cost[secondPosition[j]][j]-cost[leastPosition[j]][j];
					if(diff[j]<min_diff)
					{
						min_diff=diff[j];
						chosen_i=leastPosition[j];
						chosen_j=j;
					}
				}	
			}	
			
			// mengeprint state
			print_state(1);
			printf("maka yang terpilih adalah %d [%d][%d]\n\n",cost[chosen_i][chosen_j],chosen_i,chosen_j);
			//dummy=getchar();
			//dummy=getchar();	
			
			while(1)
			{
				 printf("=== ITERASI KE-%d ===\n", count++);
				// menghitung total alokasi tiap row
				for(i=0;i<jumlah_supply;i++)
				{
					totalize_allocation_at_row(i);
				}
				
				// buang
				if (balanced==1)
				{
					//apabila least != second
					if(cost[chosen_i][chosen_j]!=cost[secondPosition[chosen_j]][chosen_j])
					{
						satisfy_least();	
					}
					
					//apabila least==second
					else if(cost[chosen_i][chosen_j]==cost[secondPosition[chosen_j]][chosen_j])
					{
						int total_cost_least=0, total_cost_second=0;
						
						//menjumlahkan total cost least dan second
						for(j=0;j<jumlah_demand;j++)
						{
							total_cost_least+=cost[chosen_i][j];
							total_cost_second+=cost[secondPosition[chosen_j]][j];
						}
						
						// jika total second lebih banyak
						if(total_cost_least<total_cost_second)
						{
							drop_all_to_second();
						}
						
						// jika total least lebih banyak
						if(total_cost_least>total_cost_second)
						{
							satisfy_least();	
						}
					}
				}
				
				else if (balanced==0)
				{
					// jika second tidak unmet
					if(supply[secondPosition[chosen_j]]>0)
					{
						// jika	bisa satisfy second tanpa kekurangan di least, maka satisfy second dulu	
						if (total_allocation[chosen_i]-supply[secondPosition[chosen_j]]>=supply_awal[chosen_i])
						{
							satisfy_second();
						}
						
						// jika ternyata least jadi kekurangan apabila men-satisfy second, maka satisfy least dulu
						else if(total_allocation[chosen_i]-supply[secondPosition[chosen_j]]<supply_awal[chosen_i])
						{
							satisfy_least();
						}	
					}
					
					// jika second unmet, satisfy least dulu
					else if(supply[secondPosition[j]]<0)
					{
						satisfy_least();
					}
				}
				
				// mengecek status least row. apabila least row satisfy, maka cross
				if(supply[chosen_i]==0) 
				{
					cross_row(chosen_i);
					row_status[chosen_i]=0;
				}
				else if(supply[chosen_i]<0) row_status[chosen_i]=1;
				else if(supply[chosen_i]>0) row_status[chosen_i]=2;
				
				// mengecek status second row. apabila second row satisfy, maka cross
				if(supply[secondPosition[chosen_j]]==0) 
				{
					cross_row(secondPosition[chosen_j]);
					row_status[secondPosition[chosen_j]]=0;
				}
				else if(supply[secondPosition[chosen_j]]<0) row_status[secondPosition[chosen_j]]=1;
				else if(supply[secondPosition[chosen_j]]>0) row_status[secondPosition[chosen_j]]=2;
				
				// mengeprint state
				print_state(1);
						
				//apakah row saat ini masih unmet?
				if(row_status[chosen_i]!=1) break;
				else if(row_status[chosen_i]==1) 
				{						
					//mencari cell dengan diff terkecil lainnya
					min_diff=999999;
					for(j=0;j<jumlah_demand;j++)
					{
						if(allocation[chosen_i][j]>0)
						{
							// mencari second
							min=9999999;
							for(i=0;i<jumlah_supply;i++)
							{
								if(cost[i][j]<min && allocation[i][j]==-1) 
								{
									min=cost[i][j];
									secondPosition[j]=i;
								}		
							}
							diff[j]=cost[secondPosition[j]][j]-cost[chosen_i][j];
							if(diff[j]<min_diff)
							{
								min_diff=diff[j];
								chosen_j=j;
							}
						}
					}
					printf("selanjutnya yang terpilih adalah %d [%d][%d]\n\n",cost[chosen_i][chosen_j],chosen_i,chosen_j);
					//dummy=getchar();
					//dummy=getchar();
				}
			// kembali ke nomor 5			
			}
			
			// mengecek apakah masih ada row yang unmet. kalau sudah tidak ada maka break
			int akhir=1;
			for(i=0;i<jumlah_supply;i++) if (supply[i]<0) akhir=0;
			if (akhir==1) break;
		
		// kembali ke nomor 2	
		}
		print_result();
	}
				
//	system("pause");
	return 0;
}

int satisfy_least()
{
	//// least bagaimanapun masih unmet, buang semua ke second
	if (total_allocation[chosen_i]-allocation[chosen_i][chosen_j]>supply_awal[chosen_i])
	{
		allocation[secondPosition[chosen_j]][chosen_j]=	allocation[chosen_i][chosen_j];
		allocation[chosen_i][chosen_j]=0;
		supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
		supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
	}
	
	//// least di-satisfy-kan, kemudian buang sisa ke second, 
	else
	{
		int tmp=allocation[chosen_i][chosen_j];
		allocation[chosen_i][chosen_j]+=supply[chosen_i];
		allocation[secondPosition[chosen_j]][chosen_j]=tmp-allocation[chosen_i][chosen_j];
		supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
		supply[chosen_i]=0;
	}
}

int drop_all_to_second()
{
	allocation[secondPosition[chosen_j]][chosen_j]=allocation[chosen_i][chosen_j];
	allocation[chosen_i][chosen_j]=0;
	supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
	supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
}

int satisfy_second()
{
	// second row masih kurang (tidak satisfied)
	if(allocation[chosen_i][chosen_j]<supply[secondPosition[chosen_j]])
	{
		allocation[secondPosition[chosen_j]][chosen_j]=	allocation[chosen_i][chosen_j];
		allocation[chosen_i][chosen_j]=0;
		supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
		supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
	}
	
	// second row bisa satisfied
	else if(allocation[chosen_i][chosen_j]>=supply[secondPosition[chosen_j]])
	{
		allocation[secondPosition[chosen_j]][chosen_j]=supply[secondPosition[chosen_j]];
		allocation[chosen_i][chosen_j]-=allocation[secondPosition[chosen_j]][chosen_j];
		supply[secondPosition[chosen_j]]=0;
		supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
	}
}

int print_state(int indeks)
{
	printf("\n\n");
	for(p=0;p<jumlah_supply;p++)
	{
		for(q=0;q<jumlah_demand;q++)
		{
			if(allocation[p][q]==-1) printf("X\t");
			else printf("%d\t",allocation[p][q]);
		}
		printf("- %d\t",supply[p],row_status[p]);
		if(indeks==1 && row_status[p]==1) printf("(unmet)");
		printf("\n"); 
	}
	printf("\n");
	for(q=0;q<jumlah_demand;q++) printf("%d\t",demand[q]);
	if(indeks==1 && row_status[chosen_i]==1) printf("\nmasih unmet");
	printf("\n\n");
//	dummy=getchar();
//	dummy=getchar();
}

int initiate_allocation()
{
	for(j=0;j<jumlah_demand;j++)
	{
		min=99999;
		for(i=0;i<jumlah_supply;i++) 
		{
			if(cost[i][j]<min)
			{
				min_pos[j]=i;
				min=cost[i][j];
			}	
		}
		allocation[min_pos[j]][j]=demand[j];
		supply[min_pos[j]]-= allocation[min_pos[j]][j];	
	}
}

int totalize_allocation_at_row(int i)
{
	total_allocation[i]=0;
	for(j=0;j<jumlah_demand;j++) if(allocation[i][j]!=-1) total_allocation[i]+=allocation[i][j];
}

int cross_row(int i)
{
	for(j=0;j<jumlah_demand;j++) if(allocation[i][j]==-1) allocation[i][j]=0;
}

int print_result()
{
	printf("\n\n------akhir-------\n");
	int sum=0;
	for(i=0;i<jumlah_supply;i++)
	{
		for(j=0;j<jumlah_demand;j++)
		{
			if(allocation[i][j]!=-1) sum+=allocation[i][j]*cost[i][j];	
			//break;
		}	
	}
	printf("total biaya = %d\n------------------\n", sum);
	
}
			
