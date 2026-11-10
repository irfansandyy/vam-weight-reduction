#include<stdio.h>
#include<stdlib.h>


int main()
{
	int i, j, jumlah_supply, jumlah_demand, flag, p, q, total_supply=0, total_demand=0;
	char dummy;
	int count =1;
	
		
	FILE *in;
	in=fopen("n06.txt","r");
	
	while(!feof(in))
	{
		fscanf(in,"%d", &jumlah_supply);	
		fscanf(in,"%d", &jumlah_demand);

		int cost[jumlah_supply][jumlah_demand];
		int temp_cost[jumlah_supply][jumlah_demand];
		int allocation[jumlah_supply][jumlah_demand];
		int supply_awal[jumlah_supply];
		int supply[jumlah_supply];
		int demand[jumlah_demand];	
		
		// input cost, supply, dan demand
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				fscanf(in,"%d",&cost[i][j]);
				temp_cost[i][j] = cost[i][j];
//				printf("cost = %d\n",cost[i][j]);
//				printf("temp cost = %d\n",temp_cost[i][j]);
			}
		}
		
		for(i=0;i<jumlah_supply;i++)
		{
			fscanf(in,"%d",&supply[i]);
			total_supply+=supply[i];
			supply_awal[i]=supply[i];
		}
		
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
		int balanced=1;
		if (total_supply!=total_demand) balanced=0;
		
		
		// mengeprint state
//		printf("\n\n");
		printf("\n\nTabel Awal");
		printf("\n--------------------------------------------\n");
		for(p=0;p<jumlah_supply;p++)
		{
			for(q=0;q<jumlah_demand;q++)
			{
				if(allocation[p][q]==-1) printf("%d\t",cost[p][q]);
				else printf("%d\t",allocation[p][q]);
			}
			printf("| %d\n",supply[p]);
		}
		
//		printf("\n");
		for(q=0;q<jumlah_demand;q++)	printf("_______\t");
		printf("\n");
		for(q=0;q<jumlah_demand;q++)	printf("%d\t",demand[q]);
		printf("\n\n--------------------------------------------------------\n\n");
//		dummy=getchar();
//		dummy=getchar();
//		printf("\n");
		
		
		//menginisiasi, mengalokasikan demand kepada cost terkecil tiap kolom
		int min;
		int min_pos[jumlah_demand];
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
		
		// menjustifikasi unmet setiap row
		// row status :
		// 1 = unmet ( total demand > supply )
		// 2 = tidak unmet ( total demand < supply )
		int row_status[jumlah_supply], total_allocation[jumlah_supply];
		for(i=0;i<jumlah_supply;i++)
		{
			total_allocation[i]=0;
			for(j=0;j<jumlah_demand;j++)
			{
				if(allocation[i][j]!=-1) total_allocation[i]+=allocation[i][j];
			}
			if (supply[i]<0) row_status[i]=1;
			else row_status[i]=2;
		}
		
		
		while(1)
		{			
			//mencari least, second, dan diff tiap cell pada unmet row
			int leastPosition[jumlah_demand];
			int secondPosition[jumlah_demand];
			int diff[jumlah_demand];
			int chosen_i, chosen_j, min;
			int min_diff=999999;
			
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
			printf("\n");
			for(p=0;p<jumlah_supply;p++)
			{
				for(q=0;q<jumlah_demand;q++)
				{
					if(allocation[p][q]==-1) printf("%d\t",cost[p][q]);
					else printf("%d=>%d\t",temp_cost[p][q],allocation[p][q]);
//					else printf("%d\t",allocation[p][q]);
				}
				printf("| %d  (unmet=%d)\n",supply[p],row_status[p]);
			}

			for(q=0;q<jumlah_demand;q++) printf("_______\t",demand[q]);
			printf("\n");
			for(q=0;q<jumlah_demand;q++) printf("%d\t",demand[q]);
					
			printf("\n\nmaka yang terpilih adalah '%d' [%d][%d]\n",cost[chosen_i][chosen_j],chosen_i+1,chosen_j+1);
			printf("\n");
			printf("--------------------------------------------------------\n\n");
//			dummy=getchar();
//			dummy=getchar();
			
			
			while(1)
			{
				printf("=== ITERASI KE-%d ===\n", count++);

				// menghitung total alokasi tiap row
				for(i=0;i<jumlah_supply;i++)
				{
					total_allocation[i]=0;
					for(j=0;j<jumlah_demand;j++)
					{
						if(allocation[i][j]!=-1) total_allocation[i]+=allocation[i][j];
					}
				}
				
				// buang
				if (balanced==1)
				{
					//apabila least <> second & supply least != supply second
					if(cost[chosen_i][chosen_j]!=cost[secondPosition[chosen_j]][chosen_j] && supply_awal[chosen_i]!=supply_awal[secondPosition[chosen_j]])
					{
						int mod, diff_sup, diff_cost;
						diff_sup = supply_awal[chosen_i]-supply_awal[secondPosition[chosen_j]];
						diff_cost = cost[chosen_i][chosen_j] - cost[secondPosition[chosen_j]][chosen_j];
						mod = diff_sup%diff_cost;
						printf("Mod saat ini = %d\n\n",mod);
						
						// apabila second juga unmet
						if(row_status[secondPosition[chosen_j]]==1)
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
						
						// apabila mod=0
						else if(mod==0)
						{
							int total_cost_least=0, total_cost_second=0;
						
							//menjumlahkan total cost least dan second
							for(j=0;j<jumlah_demand;j++)
							{
								total_cost_least+=cost[chosen_i][j];
								total_cost_second+=cost[secondPosition[chosen_j]][j];
							}
							
							// jika total second lebih banyak
							if(total_cost_least<=total_cost_second)
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
							
							// jika total least lebih banyak
							if(total_cost_least>total_cost_second)
							{
								//// least bagaimanapun masih unmet, buang semua ke second
								if (total_allocation[chosen_i]-allocation[chosen_i][chosen_j]>supply_awal[chosen_i])
								{
									allocation[secondPosition[chosen_j]][chosen_j]=	allocation[chosen_i][chosen_j];
									allocation[chosen_i][chosen_j]=0;
									supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
									supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
									
								}
								
								// least di-satisfy-kan, kemudian buang sisa ke second
								else
								{
									int tmp=allocation[chosen_i][chosen_j];
									allocation[chosen_i][chosen_j]+=supply[chosen_i];
									allocation[secondPosition[chosen_j]][chosen_j]=tmp-allocation[chosen_i][chosen_j];
									supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
									supply[chosen_i]=0;							
								}	
							}
						}
						
						else if(mod!=0)
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
					}
					
					// apabila least <> second & supply least = supply second
					else if(cost[chosen_i][chosen_j]!=cost[secondPosition[chosen_j]][chosen_j] && supply_awal[chosen_i]==supply_awal[secondPosition[chosen_j]])
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
							allocation[secondPosition[chosen_j]][chosen_j]=allocation[chosen_i][chosen_j];
							allocation[chosen_i][chosen_j]=0;///////////////////////////////////////////////////////////////////////////////////////////////////////////////
							supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
							supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
						}
						
						// jika total least lebih banyak
						if(total_cost_least>total_cost_second)
						{
							//// least bagaimanapun masih unmet, buang semua ke second
							if (total_allocation[chosen_i]-allocation[chosen_i][chosen_j]>supply_awal[chosen_i])
							{
								allocation[secondPosition[chosen_j]][chosen_j]=	allocation[chosen_i][chosen_j];
								allocation[chosen_i][chosen_j]=0;
								supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
								supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
								
							}
							
							// least di-satisfy-kan, kemudian buang sisa ke second
							else
							{
								int tmp=allocation[chosen_i][chosen_j];
								allocation[chosen_i][chosen_j]+=supply[chosen_i];
								allocation[secondPosition[chosen_j]][chosen_j]=tmp-allocation[chosen_i][chosen_j];
								supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
								supply[chosen_i]=0;							
							}	
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
						
						// jika ternyata least jadi kekurangan apabila men-satisfy second, maka satisfy least dulu
						else if(total_allocation[chosen_i]-supply[secondPosition[chosen_j]]<supply_awal[chosen_i])
						{
							// jika bisa satisfy diri sendiri, buang sisa ke second
							if(total_allocation[chosen_i]-allocation[chosen_i][chosen_j]<=supply_awal[chosen_i])
							{
								int tmp=allocation[chosen_i][chosen_j];
								allocation[chosen_i][chosen_j]+=supply[chosen_i];
								allocation[secondPosition[chosen_j]][chosen_j]=tmp-allocation[chosen_i][chosen_j];
								supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
								supply[chosen_i]=0;
							}
							
							// jika tidak bisa satisfy diri sendiri, buang semuanya ke second
							else if(total_allocation[chosen_i]-allocation[chosen_i][chosen_j]>supply_awal[chosen_i])
							{
								allocation[secondPosition[chosen_j]][chosen_j]=allocation[chosen_i][chosen_j];
								allocation[chosen_i][chosen_j]=0;
								supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
								supply[chosen_i]+=allocation[secondPosition[chosen_j]][chosen_j];
							}
						}	
					}
					
					// jika second unmet, satisfy least dulu
					else if(supply[secondPosition[j]]<0)
					{
						int tmp=allocation[chosen_i][chosen_j];
						allocation[chosen_i][chosen_j]+=supply[chosen_i];
						allocation[secondPosition[chosen_j]][chosen_j]=tmp-allocation[chosen_i][chosen_j];
						supply[secondPosition[chosen_j]]-=allocation[secondPosition[chosen_j]][chosen_j];
						supply[chosen_i]=0;
					}
				}
				
				// mengecek status row, jika row ada yang satisfied -> cross
				if(supply[chosen_i]==0) 
				{
					for(j=0;j<jumlah_demand;j++) if(allocation[chosen_i][j]==-1) allocation[chosen_i][j]=0;
					row_status[chosen_i]=0;
				}
				else if(supply[chosen_i]<0) row_status[chosen_i]=1;
				else if(supply[chosen_i]>0) row_status[chosen_i]=2;
				
				
				if(supply[secondPosition[chosen_j]]==0) 
				{
					for(j=0;j<jumlah_demand;j++) if(allocation[secondPosition[chosen_j]][j]==-1) allocation[secondPosition[chosen_j]][j]=0;
					row_status[secondPosition[chosen_j]]=0;
				}
				else if(supply[secondPosition[chosen_j]]<0) row_status[secondPosition[chosen_j]]=1;
				else if(supply[secondPosition[chosen_j]]>0) row_status[secondPosition[chosen_j]]=2;
				
				
				// mengeprint state
				for(p=0;p<jumlah_supply;p++)
				{
					for(q=0;q<jumlah_demand;q++)
					{
						if(allocation[p][q]==-1 || allocation[p][q] == 0) printf("%d\t",cost[p][q]);
						else printf("%d=>%d\t",temp_cost[p][q],allocation[p][q]);
//						else printf("%d\t",allocation[p][q]);
					}
					printf("| %d  (unmet=%d)\n",supply[p],row_status[p]);
				}
				for(q=0;q<jumlah_demand;q++) printf("_______\t");
				printf("\n");
				for(q=0;q<jumlah_demand;q++) printf("%d\t",demand[q]);
				if(row_status[chosen_i]==1) printf("\nmasih unmet\n");
				printf("\n");
				printf("--------------------------------------------------------\n\n");
//				dummy=getchar();
//				dummy=getchar();
						
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
					printf("selanjutnya yang terpilih adalah '%d' [%d][%d]\n\n",cost[chosen_i][chosen_j],chosen_i+1,chosen_j+1);
				}			
			}
			
			// mengecek apakah masih ada row yang unmet. kalau sudah tidak ada maka break
			int akhir=1;
			for(i=0;i<jumlah_supply;i++) if (supply[i]<0) akhir=0;
			if (akhir==1) break;
			
		}
		printf("\n------akhir-------\n");
		int sum=0;
		for(i=0;i<jumlah_supply;i++)
		{
			for(j=0;j<jumlah_demand;j++)
			{
				if(allocation[i][j]!=-1) sum+=allocation[i][j]*cost[i][j];	
			}	
		}
		printf("total biaya = %d\n------------------\n", sum);
		break;
	}
	
			
//	system("pause");
	return 0;
}

